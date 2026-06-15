module napi_js;
import :api;
import :support.host;
import util;

namespace js::napi {

environment::environment(napi_env env) :
		napi_schedulable{env},
		env_{env} {
	// Initialize napi_js platform hooks
	initialize_host_environment(env);

	// nb: Closing the schedule needs to be the first thing we do, and deleting it needs to be the
	// last thing we do. Otherwise we get deadlocks or use-after-free problems.
	auto release_scheduler = [](napi_async_cleanup_hook_handle handle, void* data) -> void {
		auto finalizer = util::fn<[](napi_async_cleanup_hook_handle handle) -> void {
			napi::invoke0_noexcept(napi_remove_async_cleanup_hook, handle);
		}>;
		auto& env = *static_cast<environment*>(data);
		env.scheduler().close(util::function_ref{finalizer, handle});
	};
	napi::invoke0_noexcept(napi_add_async_cleanup_hook, env, release_scheduler, this, &cleanup_hook_handle_);
}

} // namespace js::napi
