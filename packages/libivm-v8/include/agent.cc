module;
#include <cassert>
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
export module ivm.isolated_v8:agent;
export import :agent_fwd;
import :remote_fwd;
import isolated_v8.clock;
import isolated_v8.foreground_runner;
import isolated_v8.scheduler;
import isolated_v8.task_runner;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace isolated_v8 {

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

// This keep the `weak_ptr` in `agent` alive. The `host` maintains a `weak_ptr` to this and can
// "sever" the client connection if it needs to.
class agent::severable {
	public:
		explicit severable(std::shared_ptr<host> host);

		auto sever() -> void;

	private:
		std::shared_ptr<host> host_;
};

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
class agent::host : util::non_moveable {
	private:
		struct random_seed_unlatch : util::non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		friend agent;

		explicit host(
			scheduler::layer<{}>& cluster_scheduler,
			std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
			clock::any_clock clock,
			std::optional<double> random_seed
		);
		~host();

		auto clock_time_ms() -> int64_t;
		auto foreground_runner() -> std::shared_ptr<isolated_v8::foreground_runner>;
		auto isolate() -> v8::Isolate*;
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto remote_handle_list() -> std::shared_ptr<isolated_v8::remote_handle_list>;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_specifiers() -> js::iv8::weak_map<v8::Module, js::string_t>&;

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;
		static auto make_handle(const std::shared_ptr<host>& self) -> agent;

	private:
		static auto dispose_isolate(v8::Isolate* isolate) -> void;

		std::weak_ptr<severable> severable_;
		std::shared_ptr<isolated_v8::foreground_runner> foreground_runner_;
		scheduler::runner<{}> async_scheduler_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, util::functor_of<dispose_isolate>> isolate_;
		std::shared_ptr<isolated_v8::remote_handle_list> remote_handle_list_;
		js::iv8::weak_map<v8::Module, js::string_t> weak_module_specifiers_;
		v8::Global<v8::Context> scratch_context_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

auto agent::schedule(auto task, auto... args) -> void
	requires std::invocable<decltype(task), lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto task_with_lock =
			[ host,
				task = std::move(task),
				... args = std::move(args) ](
				const std::stop_token& /*stop_token*/
			) mutable {
				agent::managed_lock agent_lock{*host};
				v8::HandleScope handle_scope{host->isolate_.get()};
				std::visit([](auto& clock) { clock.begin_tick(); }, host->clock_);
				task(agent_lock, std::move(args)...);
			};
		foreground_runner::schedule_client_task(host->foreground_runner_, task_with_lock);
	}
}

auto agent::schedule_async(auto task, auto... args) -> void
	requires std::invocable<decltype(task), const std::stop_token&, lock&, decltype(args)...> {
	auto host = host_.lock();
	if (host) {
		auto& scheduler = host->async_scheduler_;
		scheduler(
			[](
				std::stop_token stop_token,
				const std::shared_ptr<agent::host>& host,
				auto task,
				auto&&... args
			) {
				agent::managed_lock agent_lock{*host};
				v8::HandleScope handle_scope{host->isolate_.get()};
				task(std::move(stop_token), agent_lock, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

} // namespace isolated_v8
