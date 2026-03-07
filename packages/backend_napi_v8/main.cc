#include <node_api.h>
#include <tuple>
import auto_js;
import backend_napi_v8;
import napi_js;
import util;
import v8_js;
using namespace backend_napi_v8;

// Sanity check ensuring that, at least in principle, `js::transfer` can directly transfer from
// runtime to runtime without intermediates.
auto check_transfer(
	environment& napi_lock,
	napi_value napi_local,
	js::iv8::context_lock_witness v8_lock,
	v8::Local<v8::Value> v8_local
) -> void {
	[[maybe_unused]] auto v8_to_napi =
		js::transfer<napi_value>(v8_local, std::forward_as_tuple(v8_lock), std::forward_as_tuple(napi_lock));
	[[maybe_unused]] auto napi_to_v8 =
		js::transfer<v8::Local<v8::Value>>(napi_local, std::forward_as_tuple(napi_lock), std::forward_as_tuple(v8_lock));
}

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	// Initialize isolated-vm environment for this nodejs context
	auto& backend_env = environment::make(env);

	auto exports_val = js::napi::value<js::dictionary_tag>::from(exports);
	exports_val.assign(
		backend_env,
		std::tuple{
			std::pair{util::cw<"initialize">, js::forward{backend_env.make_initialize()}},
			std::pair{util::cw<"Agent">, js::forward{agent_class_template(backend_env)}},
			std::pair{util::cw<"Module">, js::forward{module_handle::class_template(backend_env)}},
			std::pair{util::cw<"Realm">, js::forward{realm_handle::class_template(backend_env)}},
			std::pair{util::cw<"Script">, js::forward{script_handle::class_template(backend_env)}},
		}
	);

	return exports;
}
