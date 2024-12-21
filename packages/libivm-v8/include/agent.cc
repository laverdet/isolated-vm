module;
#include <concepts>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
export module ivm.isolated_v8:agent;
export import :agent_fwd;
import :platform.clock;
import :platform.foreground_runner;
import :scheduler;
import ivm.iv8;
import ivm.utility;
import v8;

namespace ivm {

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock : util::non_moveable, public util::pointer_facade<agent::lock> {
	public:
		explicit lock(host& host);
		auto operator*(this auto& self) -> decltype(auto) { return self.host_; }

	private:
		host& host_;
};

// A lock explicitly created on this stack
class agent::managed_lock : public agent::lock {
	public:
		explicit managed_lock(host& host);

	private:
		v8::Locker locker_;
		v8::Isolate::Scope scope_;
};

// This is constructed before (and may outlive) an agent
class agent::storage : util::non_moveable {
	public:
		explicit storage(scheduler& scheduler);

		auto scheduler_handle() -> scheduler::handle&;

	private:
		scheduler::handle scheduler_handle_;
};

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
class agent::host : util::non_moveable {
	private:
		struct isolate_destructor {
				auto operator()(v8::Isolate* isolate) -> void;
		};

		struct random_seed_unlatch : util::non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		friend agent;

		explicit host(
			std::shared_ptr<storage> agent_storage,
			std::shared_ptr<foreground_runner> task_runner,
			clock::any_clock clock,
			std::optional<double> random_seed
		);

		auto clock_time_ms() -> int64_t;
		auto execute(const std::stop_token& stop_token) -> void;
		auto isolate() -> v8::Isolate*;
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto run_task(std::invocable<lock&> auto task) -> void;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_specifiers() -> iv8::weak_map<v8::Module, value::string_t>&;

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;

	private:
		std::shared_ptr<storage> agent_storage_;
		std::shared_ptr<foreground_runner> task_runner_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		iv8::weak_map<v8::Module, value::string_t> weak_module_specifiers_;
		std::unique_ptr<v8::Isolate, isolate_destructor> isolate_;
		v8::Global<v8::Context> scratch_context_;
		scheduler async_scheduler_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

// Allow lambda-style callbacks to be called with the same virtual dispatch as `v8::Task`
template <std::invocable<agent::lock&> Invocable>
struct task_of : v8::Task {
		explicit task_of(Invocable task) :
				task_{std::move(task)} {}

		auto Run() -> void final {
			auto* host = agent::host::get_current();
			if (host == nullptr) {
				throw std::logic_error("Task invoked outside of agent context");
			}
			agent::lock lock{*host};
			task_(lock);
		}

	private:
		[[no_unique_address]] Invocable task_;
};

auto agent::host::run_task(std::invocable<lock&> auto task) -> void {
	agent::managed_lock agent_lock{*this};
	const auto handle_scope = v8::HandleScope{isolate_.get()};
	task(agent_lock);
}

auto agent::schedule_async(std::invocable<const std::stop_token&, lock&> auto task) -> void {
	auto host = host_.lock();
	if (host) {
		auto& handle = host->agent_storage_->scheduler_handle();
		host->async_scheduler_.run(
			[ host = std::move(host),
				task = std::move(task) ](
				const std::stop_token& stop_token
			) mutable {
				host->run_task(
					[ task = std::move(task),
						&stop_token ](
						lock& agent
					) mutable {
						task(stop_token, agent);
					}
				);
			},
			handle
		);
	}
}

auto agent::schedule(std::invocable<lock&> auto task) -> void {
	task_runner_->schedule_non_nestable(std::make_unique<task_of<decltype(task)>>(std::move(task)));
}

} // namespace ivm
