module;
#include <cassert>
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
export module ivm.isolated_v8:agent;
export import :agent_fwd;
import isolated_v8.clock;
import isolated_v8.foreground_runner;
import isolated_v8.lock;
import isolated_v8.remote_handle;
import isolated_v8.remote_handle_list;
import isolated_v8.scheduler;
import isolated_v8.task_runner;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace isolated_v8 {

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock final
		: public util::pointer_facade<agent::lock>,
			public isolate_scope_lock,
			public remote_handle_lock {
	public:
		explicit lock(std::shared_ptr<agent::host> host);

		auto operator*(this auto& self) -> decltype(auto) { return *self.host_; }
		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;

	private:
		std::shared_ptr<host> host_;
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
class agent::host final {
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
		auto foreground_runner() -> std::shared_ptr<isolated_v8::foreground_runner> { return foreground_runner_; }
		auto isolate() -> v8::Isolate* { return isolate_.get(); }
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto remote_handle_list() -> isolated_v8::remote_handle_list& { return remote_handle_list_; }
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_specifiers() -> js::iv8::weak_map<v8::Module, js::string_t>& { return weak_module_specifiers_; }

		static auto get_current() -> host*;
		static auto get_current(v8::Isolate* isolate) -> host&;
		static auto make_handle(const std::shared_ptr<host>& self) -> agent;

	private:
		static auto dispose_isolate(v8::Isolate* isolate) -> void;

		std::weak_ptr<severable> severable_;
		std::shared_ptr<isolated_v8::foreground_runner> foreground_runner_;
		scheduler::runner<{}> async_scheduler_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, util::function_type_of<dispose_isolate>> isolate_;
		isolated_v8::remote_handle_list remote_handle_list_;
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
				agent::lock agent_lock{host};
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
				agent::lock agent_lock{host};
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
