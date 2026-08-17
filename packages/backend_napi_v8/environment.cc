module backend_napi_v8;
import :environment;
import :native_module;
using namespace js;

namespace backend_napi_v8 {

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

} // namespace backend_napi_v8
