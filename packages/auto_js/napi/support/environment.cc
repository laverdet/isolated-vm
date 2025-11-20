module;
#include <stdexcept>
module napi_js;
import :api;
import :utility;
import util;

namespace js::napi {

environment::environment(napi_env env) :
		env_{env},
		uses_direct_handles_{[ & ]() -> bool {
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
		}()} {
	scheduler().open(napi::invoke(napi_get_uv_event_loop, env));
}

environment::~environment() {
	scheduler().close();
}

} // namespace js::napi
