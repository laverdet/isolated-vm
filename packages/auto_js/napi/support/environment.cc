module napi_js;
import :api;
import :utility;
import util;

namespace js::napi {

environment::environment(napi_env env, bool uses_direct_handles) :
		napi_schedulable{env},
		env_{env},
		address_equal_{uses_direct_handles},
		uses_direct_handles_{uses_direct_handles} {
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

environment::environment(napi_env env) :
		environment{
			env,
			[ & ]() -> bool {
				const auto direct_equal = direct_address_equal{};
				const auto indirect_equal = indirect_address_equal{};
				auto* array = napi::invoke(napi_create_array_with_length, env, 1);
				napi::invoke0(napi_set_element, env, array, 0, array);
				auto* result = napi::invoke(napi_get_element, env, array, 0);
				if (direct_equal(array, result)) {
					return true;
				} else if (indirect_equal(array, result)) {
					return false;
				} else {
					throw std::runtime_error{"Exotic napi handle behavior detected"};
				}
			}()
		} {}

} // namespace js::napi
