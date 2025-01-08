module;
#include <cassert>
#include <concepts>
#include <forward_list>
#include <memory>
#include <optional>
#include <stop_token>
export module ivm.isolated_v8:agent;
export import :agent_fwd;
import :platform.clock;
import :scheduler;
import ivm.iv8;
import ivm.utility;
import v8;

namespace ivm {

export using cluster_scheduler = scheduler::layer<{}>;

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
		using agent_scheduler = scheduler::runner<{}>;
		explicit storage(ivm::cluster_scheduler& cluster_scheduler);

		auto scheduler() -> agent_scheduler& { return scheduler_; }

	private:
		agent_scheduler scheduler_;
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
		auto execute(std::stop_token stop_token) -> void;
		auto isolate() -> v8::Isolate*;
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_specifiers() -> js::iv8::weak_map<v8::Module, js::string_t>&;

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;

	private:
		std::shared_ptr<storage> agent_storage_;
		std::shared_ptr<foreground_runner> task_runner_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		js::iv8::weak_map<v8::Module, js::string_t> weak_module_specifiers_;
		std::unique_ptr<v8::Isolate, isolate_destructor> isolate_;
		v8::Global<v8::Context> scratch_context_;
		util::lockable<std::forward_list<scheduler::handle>> async_task_handles_;

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
			assert(host != nullptr);
			agent::lock lock{*host};
			task_(lock);
		}

	private:
		[[no_unique_address]] Invocable task_;
};

auto agent::schedule_async(std::invocable<const std::stop_token&, lock&> auto task) -> void {
	auto host = host_.lock();
	if (host) {
		auto& scheduler = host->agent_storage_->scheduler();
		scheduler(
			[](
				std::stop_token stop_token,
				const std::shared_ptr<agent::host>& host,
				auto task
			) {
				agent::managed_lock agent_lock{*host};
				v8::HandleScope handle_scope{host->isolate_.get()};
				task(std::move(stop_token), agent_lock);
			},
			std::move(host),
			std::move(task)
		);
	}
}

auto agent::schedule(std::invocable<lock&> auto task) -> void {
	task_runner_->schedule_non_nestable(std::make_unique<task_of<decltype(task)>>(std::move(task)));
}

} // namespace ivm
