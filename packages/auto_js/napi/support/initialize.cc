module;
#include <cassert>
module napi_js;
import :initialize;

namespace js::napi::detail {

initialize_require* module_to_initialize = nullptr;

initialize_require::initialize_require(int /*nothing*/, initialize_require& instance) {
	assert(module_to_initialize == nullptr);
	module_to_initialize = &instance;
}

initialize_require::~initialize_require() {
	assert(module_to_initialize == this);
	module_to_initialize = nullptr;
}

auto initialize_require::initialize(napi_env env, napi_value exports) -> void {
	assert(module_to_initialize != nullptr);
	(*module_to_initialize)(env, exports);
}

} // namespace js::napi::detail
