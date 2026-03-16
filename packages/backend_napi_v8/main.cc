#include <napi_js_initialize.h>
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

// Initialize this module
js::napi::napi_js_module module_namespace{
	std::type_identity<environment>{},
	[](environment& env) -> auto {
		return std::tuple{
			std::in_place,
			std::pair{util::cw<"initialize">, js::forward{env.make_initialize()}},
			std::pair{util::cw<"Agent">, js::forward{agent_class_template(env)}},
			std::pair{util::cw<"Module">, js::forward{module_handle::class_template(env)}},
			std::pair{util::cw<"Realm">, js::forward{realm_handle::class_template(env)}},
			std::pair{util::cw<"Script">, js::forward{script_handle::class_template(env)}},
		};
	}
};
