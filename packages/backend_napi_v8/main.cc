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
	auto& backend_env = environment::make(env);

	auto exports_val = js::napi::value<js::dictionary_tag>::from(exports);
	exports_val.assign(
		backend_env,
		std::tuple{
			std::pair{util::string_literal{"compileModule"}, js::transfer_direct{make_compile_module(backend_env)}},
			std::pair{util::string_literal{"compileScript"}, js::transfer_direct{make_compile_script(backend_env)}},
			std::pair{util::string_literal{"createAgent"}, js::transfer_direct{make_create_agent(backend_env)}},
			std::pair{util::string_literal{"createCapability"}, js::transfer_direct{make_create_capability(backend_env)}},
			std::pair{util::string_literal{"createRealm"}, js::transfer_direct{make_create_realm(backend_env)}},
			std::pair{util::string_literal{"evaluateModule"}, js::transfer_direct{make_evaluate_module(backend_env)}},
			std::pair{util::string_literal{"linkModule"}, js::transfer_direct{make_link_module(backend_env)}},
			std::pair{util::string_literal{"runScript"}, js::transfer_direct{make_run_script(backend_env)}}
		}
	);

	return exports;
}
