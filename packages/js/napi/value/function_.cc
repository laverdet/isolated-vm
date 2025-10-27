module;
#include <array>
#include <span>
#include <tuple>
#include <variant>
#include <vector>
export module napi_js:function;
import :api;
import :callback;
import :environment;
import :finalizer;
import :primitive;
import :value;
import isolated_js;
import ivm.utility;

namespace js::napi {

template <>
class value<function_tag> : public detail::value_next<function_tag> {
	public:
		using detail::value_next<function_tag>::value_next;

		template <class Result>
		auto apply(auto& env, auto&& args) -> Result;
		template <class Result>
		auto call(auto& env, auto&&... args) -> Result;

		template <class Environment>
		static auto make(Environment& env, auto function) -> value<function_tag>;

	private:
		template <class Result>
		auto invoke(auto& env, std::span<napi_value> args) -> Result;
};

// ---

template <class Result>
auto value<function_tag>::apply(auto& env, auto&& args) -> Result {
	auto arg_values = js::transfer_in_strict<std::vector<napi_value>>(std::forward<decltype(args)>(args), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value<function_tag>::call(auto& env, auto&&... args) -> Result {
	auto arg_values = js::transfer_in_strict<std::array<napi_value, sizeof...(args)>>(std::forward_as_tuple(args...), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value<function_tag>::invoke(auto& env, std::span<napi_value> args) -> Result {
	auto undefined = js::transfer_in_strict<napi_value>(std::monostate{}, env);
	auto* result = js::napi::invoke(napi_call_function, napi_env{env}, undefined, napi_value{*this}, args.size(), args.data());
	return js::transfer_out<Result>(result, env);
}

template <class Environment>
auto value<function_tag>::make(Environment& env, auto function) -> value<function_tag> {
	auto [ callback, data, finalizer ] = make_napi_callback(env, make_free_function<Environment>(std::move(function).callback));
	const auto make = [ & ]() -> value<function_tag> {
		return value<function_tag>::from(js::napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, data));
	};
	if constexpr (type<decltype(finalizer)> == type<std::nullptr_t>) {
		// No finalizer needed
		return make();
	} else {
		// Function requires finalizer
		return js::napi::apply_finalizer(std::move(finalizer), [ & ](auto* data, napi_finalize finalize, void* hint) -> value<function_tag> {
			auto js_function = make();
			js::napi::invoke0(napi_add_finalizer, napi_env{env}, js_function, data, finalize, hint, nullptr);
			return js_function;
		});
	}
}

} // namespace js::napi
