module;
#include <cstring>
module ivm.node;
import :environment;
import ivm.isolated_v8;
import ivm.js;
import ivm.napi;
import ivm.utility;
import napi;
import v8;

namespace ivm {

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

} // namespace ivm
