#include <node_api.h>
#include <memory>
import ivm.napi;
import ivm.node;
import napi;

using namespace ivm;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto ienv = std::make_unique<environment>(env);

	napi_check_result_of(napi_set_property, env, exports, ivm::napi::create_string_literal<"compileModule">(env), make_compile_module(*ienv));
	napi_check_result_of(napi_set_property, env, exports, ivm::napi::create_string_literal<"compileScript">(env), make_compile_script(*ienv));
	napi_check_result_of(napi_set_property, env, exports, ivm::napi::create_string_literal<"createAgent">(env), make_create_agent(*ienv));
	napi_check_result_of(napi_set_property, env, exports, ivm::napi::create_string_literal<"createRealm">(env), make_create_realm(*ienv));
	napi_check_result_of(napi_set_property, env, exports, ivm::napi::create_string_literal<"runScript">(env), make_run_script(*ienv));

	napi_check_result_of(napi_set_instance_data, env, ienv.get(), nullptr, nullptr);
	napi_check_result_of(
		napi_set_instance_data,
		env,
		ienv.release(),
		[](napi_env /*env*/, void* data, void* /*hint*/) { delete static_cast<environment*>(data); },
		nullptr
	);
	return exports;
}
