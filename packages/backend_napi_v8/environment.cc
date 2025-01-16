module;
#include <cstring>
module backend_napi_v8.environment;
import ivm.js;
import ivm.napi;
import ivm.utility;
import nodejs;
import v8;

namespace backend_napi_v8 {

environment::environment(napi_env env) :
		env_{env},
		isolate_{v8::Isolate::GetCurrent()} {
	scheduler_.open(js::napi::invoke(napi_get_uv_event_loop, env));
}

environment::~environment() {
	scheduler_.close();
}

auto environment::get(napi_env env) -> environment& {
	return *static_cast<environment*>(js::napi::invoke(napi_get_instance_data, env));
}

} // namespace backend_napi_v8
