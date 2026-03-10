#include <node_api.h>
import napi_js;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	js::napi::detail::initialize_require::initialize(env, exports);
	return exports;
}
