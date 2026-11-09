module;
#include "runtime_dist_runtime_cjs.h"
module napi_js;
import :api;
import :reference;
import :runtime;
import :value;
import std;
import util;

namespace js::napi {

runtime::runtime(napi_env env) {
	// Invoke as minimal CommonJS module
	auto source = std::format("(function(exports, module) {{\n{}\n}})", util::make_consteval_string_view(runtime_dist_runtime_cjs));
	auto* wrapper = napi::invoke(napi_run_script, env, napi::invoke(napi_create_string_latin1, env, source.data(), source.size()));
	auto* exports = napi::invoke(napi_create_object, env);
	auto* module = napi::invoke(napi_create_object, env);
	napi::invoke0(napi_set_named_property, env, module, "exports", exports);
	auto argv = std::array<napi_value, 2>{exports, module};
	std::ignore = napi::invoke(napi_call_function, env, napi::invoke(napi_get_undefined, env), wrapper, argv.size(), argv.data());
	exports = napi::invoke(napi_get_named_property, env, module, "exports");

	// Extract runtime functions
	auto function = [ & ](const char* name) -> reference<function_tag> {
		auto* value = napi::invoke(napi_get_named_property, env, exports, name);
		return reference<function_tag>{env, local_of<function_tag>::from(value)};
	};
	async_to_callbacks_ = function("asyncToCallbacks");
}

auto runtime::async_to_callbacks(napi_env env) const -> local_of<function_tag> {
	return async_to_callbacks_.get(env);
}

} // namespace js::napi
