module;
#include <cstring>
module backend_napi_v8;
import isolated_js;
import ivm.utility;
import napi_js;
import nodejs;

namespace backend_napi_v8 {

environment::environment(napi_env env) :
		environment_of{env} {
	scheduler().open(js::napi::invoke(napi_get_uv_event_loop, env));
}

environment::~environment() {
	scheduler().close();
}

auto environment::get(napi_env env) -> environment& {
	return *static_cast<environment*>(js::napi::invoke(napi_get_instance_data, env));
}

} // namespace backend_napi_v8
