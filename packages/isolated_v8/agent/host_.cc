module;
#include <memory>
#include <optional>
export module isolated_v8:agent_host;
import :agent_fwd;
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

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
export class agent_host {
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
		auto remote_handle_list() -> isolated_v8::remote_handle_list& { return remote_handle_list_; }
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;
		auto weak_module_actions() -> weak_modules_actions_type& { return weak_module_actions_; }
		auto weak_module_specifiers() -> weak_modules_specifiers_type& { return weak_module_specifiers_; }

		static auto get_current() -> agent_host*;
		static auto get_current(v8::Isolate* isolate) -> agent_host& { return *static_cast<agent_host*>(isolate->GetData(0)); }
		static auto make_handle(std::shared_ptr<agent_host> self) -> agent_handle;

	private:
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
		v8::Global<v8::Context> scratch_context_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

} // namespace isolated_v8
