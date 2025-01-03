#include <node_api.h>
#include <memory>
#include <tuple>
import ivm.napi;
import ivm.node;
import ivm.js;
import napi;

using namespace ivm;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto ienv = std::make_unique<environment>(env);

	js::napi::object::assign(
		env,
		js::napi::object{env, js::napi::value<js::object_tag>::from(exports)},
		std::tuple{
			std::pair{js::string_literal<"compileModule">{}, js::transfer_direct{make_compile_module(*ienv)}},
			std::pair{js::string_literal<"compileScript">{}, js::transfer_direct{make_compile_script(*ienv)}},
			std::pair{js::string_literal<"createAgent">{}, js::transfer_direct{make_create_agent(*ienv)}},
			std::pair{js::string_literal<"createRealm">{}, js::transfer_direct{make_create_realm(*ienv)}},
			std::pair{js::string_literal<"linkModule">{}, js::transfer_direct{make_link_module(*ienv)}},
			std::pair{js::string_literal<"runScript">{}, js::transfer_direct{make_run_script(*ienv)}}
		}
	);

	js::napi::invoke0(
		napi_set_instance_data,
		env,
		ienv.release(),
		[](napi_env /*env*/, void* data, void* /*hint*/) { delete static_cast<environment*>(data); },
		nullptr
	);
	return exports;
}
