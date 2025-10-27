module;
#include <array>
#include <span>
#include <tuple>
#include <variant>
#include <vector>
export module napi_js:function_definitions;
import :api;
import :callback;
import :function;
import ivm.utility;

namespace js::napi {

template <class Result>
auto value<function_tag>::apply(auto_environment auto& env, auto&& args) -> Result {
	auto arg_values = js::transfer_in_strict<std::vector<napi_value>>(std::forward<decltype(args)>(args), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value<function_tag>::call(auto_environment auto& env, auto&&... args) -> Result {
	auto arg_values = js::transfer_in_strict<std::array<napi_value, sizeof...(args)>>(std::forward_as_tuple(args...), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value<function_tag>::invoke(auto_environment auto& env, std::span<napi_value> args) -> Result {
	auto undefined = js::transfer_in_strict<napi_value>(std::monostate{}, env);
	auto* result = napi::invoke(napi_call_function, napi_env{env}, undefined, napi_value{*this}, args.size(), args.data());
	return js::transfer_out<Result>(result, env);
}

template <auto_environment Environment>
auto value<function_tag>::make(Environment& env, auto function) -> value<function_tag> {
	auto [ callback, data, finalizer ] = make_napi_callback(env, make_free_function<Environment>(std::move(function).callback));
	const auto make = [ & ]() -> value<function_tag> {
		return value<function_tag>::from(napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, data));
	};
	if constexpr (type<decltype(finalizer)> == type<std::nullptr_t>) {
		// No finalizer needed
		return make();
	} else {
		// Function requires finalizer
		return apply_finalizer(std::move(finalizer), [ & ](auto* data, napi_finalize finalize, void* hint) -> value<function_tag> {
			auto js_function = make();
			napi::invoke0(napi_add_finalizer, napi_env{env}, js_function, data, finalize, hint, nullptr);
			return js_function;
		});
	}
}

} // namespace js::napi
