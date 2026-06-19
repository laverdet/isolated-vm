module backend_napi_v8;
import :environment;
import :native_module;
using namespace js;

namespace backend_napi_v8 {

struct inherited_classes {
		js::forward<napi::value_of<function_tag>> agent;
		js::forward<napi::value_of<function_tag>> module_;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"Agent">, &inherited_classes::agent},
			js::struct_member{util::cw<"Module">, &inherited_classes::module_},
		};
};

environment::environment(napi_env env) :
		napi::environment{env},
		cluster_{util::function_ref{util::fn<&environment::destroy_orphan_scheduler>, *this}} {}

environment::~environment() {
	native_module_handle::unload_hook();
}

auto environment::destroy_orphan_scheduler(std::any isolate_scheduler) -> void {
	scheduler()(
		[ isolate_scheduler = std::move(isolate_scheduler) ](
			napi_env /*env*/, napi_value /*nothing*/
		) -> void {
			// nb: If an agent is disposed while there is no work then the destructor will run directly in
			// the nodejs thread. In that case the destructor `join`'s with its terminating foreground
			// thread, the happy case. If nodejs requests a dispose while the agent is working then it
			// will hold the last reference to its own scheduler. When it detects that the destructor is
			// running on its own foreground thread it sends the thread back over to nodejs, here, to be
			// joined with.
		}
	);
}

auto environment::make_initialize() -> napi::value_of<function_tag> {
	auto initialize = [](environment& env, inherited_classes inherited) -> void {
		env.agent_class_ = napi::reference{env, *inherited.agent};
		env.module_class_ = napi::reference{env, *inherited.module_};
	};
	return napi::value_of<function_tag>::make(*this, js::free_function{initialize});
}

} // namespace backend_napi_v8
