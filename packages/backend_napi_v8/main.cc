#include <node_api.h>
#include <tuple>
import napi_js;
import backend_napi_v8;
import isolated_js;
import ivm.utility;
import nodejs;
using namespace backend_napi_v8;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto& ienv = environment::make(env);

	js::napi::object::assign(
		ienv,
		js::napi::object{env, js::napi::value<js::object_tag>::from(exports)},
		std::tuple{
			std::pair{util::string_literal{"compileModule"}, js::transfer_direct{make_compile_module(ienv)}},
			std::pair{util::string_literal{"compileScript"}, js::transfer_direct{make_compile_script(ienv)}},
			std::pair{util::string_literal{"createAgent"}, js::transfer_direct{make_create_agent(ienv)}},
			std::pair{util::string_literal{"createCapability"}, js::transfer_direct{make_create_capability(ienv)}},
			std::pair{util::string_literal{"createRealm"}, js::transfer_direct{make_create_realm(ienv)}},
			std::pair{util::string_literal{"evaluateModule"}, js::transfer_direct{make_evaluate_module(ienv)}},
			std::pair{util::string_literal{"linkModule"}, js::transfer_direct{make_link_module(ienv)}},
			std::pair{util::string_literal{"runScript"}, js::transfer_direct{make_run_script(ienv)}}
		}
	);

	return exports;
}
