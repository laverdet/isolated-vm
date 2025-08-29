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

// Helper specializing against a function signature to transfer out the arguments, and transfer in
// the result.
template <class Signature>
struct invoke_callback;

template <class Result, class Environment, class... Args>
struct invoke_callback<Result(Environment, Args...)> {
		auto operator()(auto& env, const callback_info& info, auto& invocable) {
			auto run = util::regular_return{[ & ]() -> decltype(auto) {
				return std::apply(
					invocable,
					std::tuple_cat(
						std::forward_as_tuple(env),
						js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
					)
				);
			}};
			return js::transfer_in_strict<napi_value>(run(), env);
		}
};

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
	using function_type = std::decay_t<decltype(function.callback)>;
	using signature_type = util::function_signature_t<function_type>;
	auto trampoline = util::bind_parameters{
		[](auto& callback, Environment& env, const callback_info& info) -> napi_value {
			return invoke_callback<signature_type>{}(env, info, callback);
		},
		std::move(function.callback)
	};
	auto [ callback_ptr, finalizer ] = make_napi_callback(env, std::move(trampoline));
	const auto make = [ & ]() {
		return value<function_tag>::from(js::napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback_ptr.first, callback_ptr.second));
	};
	if constexpr (std::is_same_v<decltype(finalizer), std::nullptr_t>) {
		// No finalizer needed
		return make();
	} else {
		// Function requires finalizer
		return js::napi::apply_finalizer(std::move(finalizer), [ & ](auto* data, napi_finalize finalize, void* hint) {
			auto js_function = make();
			js::napi::invoke0(napi_add_finalizer, napi_env{env}, js_function, data, finalize, hint, nullptr);
			return js_function;
		});
	}
}

} // namespace js::napi
