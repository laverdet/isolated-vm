#include <node_api.h>
#include <memory>
import ivm.napi;
import ivm.node;
import ivm.js;
import napi;

using namespace ivm;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto ienv = std::make_unique<environment>(env);

	js::napi::invoke0(napi_set_property, env, exports, js::napi::value<js::string_literal_tag<"compileModule">>::make(env), make_compile_module(*ienv));
	js::napi::invoke0(napi_set_property, env, exports, js::napi::value<js::string_literal_tag<"compileScript">>::make(env), make_compile_script(*ienv));
	js::napi::invoke0(napi_set_property, env, exports, js::napi::value<js::string_literal_tag<"createAgent">>::make(env), make_create_agent(*ienv));
	js::napi::invoke0(napi_set_property, env, exports, js::napi::value<js::string_literal_tag<"createRealm">>::make(env), make_create_realm(*ienv));
	js::napi::invoke0(napi_set_property, env, exports, js::napi::value<js::string_literal_tag<"runScript">>::make(env), make_run_script(*ienv));

	js::napi::invoke0(
		napi_set_instance_data,
		env,
		ienv.release(),
		[](napi_env /*env*/, void* data, void* /*hint*/) { delete static_cast<environment*>(data); },
		nullptr
	);
	return exports;
}
