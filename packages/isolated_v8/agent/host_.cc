module;
#include <memory>
#include <optional>
export module isolated_v8:agent_host;
import :clock;
import :evaluation_module_action;
import :foreground_runner;
import :remote_handle_list;
import :scheduler;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export class agent_host;

export struct behavior_params {
		clock::any_clock clock;
		std::optional<double> random_seed;
};

// This keeps the `weak_ptr` in `agent_handle` alive. The `agent_host` maintains a `weak_ptr` to
// this and can "sever" the client connection if it needs to.
// TODO: This could be handled with an internal counter on the `agent_host` instead of creating a
// separate shared_ptr control block.
export class agent_severable {
	public:
		explicit agent_severable(std::shared_ptr<agent_host> host);
		auto sever() -> void;

	private:
		std::atomic<std::shared_ptr<agent_host>> host_;
};

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
export class agent_host : public std::enable_shared_from_this<agent_host> {
	private:
		using weak_modules_actions_type = js::iv8::weak_map<v8::Module, synthetic_module_action_type>;
		using weak_modules_specifiers_type = js::iv8::weak_map<v8::Module, js::string_t>;

		struct random_seed_unlatch : util::non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		explicit agent_host(
			scheduler::layer<{}>& cluster_scheduler,
			std::shared_ptr<isolated_v8::foreground_runner> foreground_runner,
			behavior_params params
		);
		~agent_host();

		auto async_scheduler(this auto& self) -> auto& { return self.async_scheduler_; }
		auto autorelease_pool() -> util::autorelease_pool& { return autorelease_pool_; }
		auto clock(this auto& self) -> auto& { return self.clock_; }
		auto clock_time_ms() -> int64_t;
		auto foreground_runner(this auto& self) -> auto& { return self.foreground_runner_; }
		auto isolate() -> v8::Isolate* { return isolate_.get(); }
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto remote_handle_lock() -> isolated_v8::remote_handle_lock;
		auto remote_handle_list() -> auto& { return remote_handle_list_; }
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_actions() -> weak_modules_actions_type& { return weak_module_actions_; }
		auto weak_module_specifiers() -> weak_modules_specifiers_type& { return weak_module_specifiers_; }

		static auto acquire_severable(const std::shared_ptr<agent_host>& self) -> std::shared_ptr<agent_severable>;
		static auto get_current() -> agent_host*;
		static auto get_current(v8::Isolate* isolate) -> agent_host& { return *static_cast<agent_host*>(isolate->GetData(0)); }

	private:
		auto reset_remote_handle(expired_remote_type remote) -> void;
		static auto dispose_isolate(v8::Isolate* isolate) -> void { isolate->Dispose(); }

		util::autorelease_pool autorelease_pool_;
		std::weak_ptr<agent_severable> severable_;
		std::shared_ptr<isolated_v8::foreground_runner> foreground_runner_;
		scheduler::runner<{}> async_scheduler_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, util::invocable_constant<dispose_isolate>> isolate_;
		isolated_v8::remote_handle_list remote_handle_list_;
		weak_modules_actions_type weak_module_actions_;
		weak_modules_specifiers_type weak_module_specifiers_;
		reset_handle_type remote_expiration_callback_;
		v8::Global<v8::Context> scratch_context_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

// Agent which also manages environment storage of a given type.
export template <class Type>
class agent_host_of;

template <class Type>
class agent_host_of : public agent_host {
	public:
		explicit agent_host_of(auto make_environment, auto&&... creation_args)
			requires std::constructible_from<agent_host, decltype(creation_args)...> :
				agent_host{std::forward<decltype(creation_args)>(creation_args)...},
				environment_{make_environment()} {}

		auto environment(this auto& self) -> auto& { return self.environment_; }

	private:
		Type environment_;
};

} // namespace isolated_v8
