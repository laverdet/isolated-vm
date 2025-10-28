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
			std::pair{util::cw<"Agent">, js::forward{agent_class_template(backend_env)}},
			std::pair{util::cw<"Module">, js::forward{module_handle::class_template(backend_env)}},
			std::pair{util::cw<"Realm">, js::forward{realm_handle::class_template(backend_env)}},
			std::pair{util::cw<"Script">, js::forward{script_handle::class_template(backend_env)}},
		}
	);

	return exports;
}
