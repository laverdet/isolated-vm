export module napi_js:function_definitions;
import :callback;
import std;

namespace js::napi {

template <class Result>
auto value_for_function::apply(auto_environment auto& env, auto&& args) -> Result {
	auto arg_values = js::transfer_in_strict<std::vector<napi_value>>(std::forward<decltype(args)>(args), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value_for_function::call(auto_environment auto& env, auto&&... args) -> Result {
	auto arg_values = js::transfer_in_strict<std::array<napi_value, sizeof...(args)>>(std::forward_as_tuple(args...), env);
	return invoke<Result>(env, std::span{arg_values});
}

template <class Result>
auto value_for_function::invoke(auto_environment auto& env, std::span<napi_value> args) -> Result {
	auto undefined = js::transfer_in_strict<napi_value>(std::monostate{}, env);
	auto* result = napi::invoke(napi_call_function, napi_env{env}, undefined, napi_value{*this}, args.size(), args.data());
	return js::transfer_out<Result>(result, env);
}

template <auto_environment Environment>
auto value_for_function::make(Environment& env, auto function) -> value_of<function_tag> {
	auto [ callback, data ] = make_callback_storage(env, make_free_function<Environment>(std::move(function).callback));
	auto make = [ & ](void* data) -> value_of<function_tag> {
		return value_of<function_tag>::from(napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, data));
	};
	if constexpr (requires { typename decltype(data)::element_type; }) {
		// Function requires finalizer
		auto function = make(data.get());
		return apply_finalizer(std::move(data), [ & ](auto* data, napi_finalize finalize, void* hint) -> value_of<function_tag> {
			napi::invoke0(napi_add_finalizer, napi_env{env}, function, data, finalize, hint, nullptr);
			return function;
		});
	} else {
		// No finalizer needed
		return make(data);
	}
}

} // namespace js::napi
