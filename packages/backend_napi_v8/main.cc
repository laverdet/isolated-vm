#include <node_api.h>
#include <tuple>
import napi_js;
import backend_napi_v8;
import isolated_js;
import ivm.utility;
import nodejs;
using namespace backend_napi_v8;
using namespace util::string_literals;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto& backend_env = environment::make(env);

	auto exports_val = js::napi::value<js::dictionary_tag>::from(exports);
	exports_val.assign(
		backend_env,
		std::tuple{
			std::pair{"compileModule"_sl, js::forward{module_handle::make_compile_module(backend_env)}},
			std::pair{"compileScript"_sl, js::forward{make_compile_script(backend_env)}},
			std::pair{"createAgent"_sl, js::forward{agent_handle::make_create_agent(backend_env)}},
			std::pair{"createCapability"_sl, js::forward{module_handle::make_create_capability(backend_env)}},
			std::pair{"createRealm"_sl, js::forward{realm_handle::make_create_realm(backend_env)}},
			std::pair{"evaluateModule"_sl, js::forward{module_handle::make_evaluate_module(backend_env)}},
			std::pair{"linkModule"_sl, js::forward{module_handle::make_link_module(backend_env)}},
			std::pair{"runScript"_sl, js::forward{make_run_script(backend_env)}}
		}
	);

	return exports;
}
