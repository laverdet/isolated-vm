#include <node_api.h>
// nb: Weird clang + libstdc++ issue with this one duplicated symbol. Probably some kind of module
// issue. Including `cmath` fixes it.
// /usr/lib/llvm-22/include/c++/v1/__type_traits/promote.h:38:1: error: redefinition of '__promote_t' as different kind of symbol
//    38 | using __promote_t _LIBCPP_NODEBUG =
//       | ^
// /usr/lib/llvm-22/include/c++/v1/__type_traits/promote.h:38:1: note: previous definition is here
//    38 | using __promote_t _LIBCPP_NODEBUG =
#include <cmath>
import napi_js;

NAPI_MODULE_INIT(/*napi_env env, napi_value exports*/) {
	js::napi::detail::initialize_require::initialize(env, exports);
	return exports;
}
