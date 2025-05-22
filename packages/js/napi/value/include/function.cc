module;
#include <array>
#include <memory>
#include <span>
#include <tuple>
#include <variant>
#include <vector>
export module napi_js.function;
import isolated_js;
import ivm.utility;
import napi_js.callback_info;
import napi_js.environment;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;

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
		static auto make(Environment& env, auto&& function) -> value<function_tag>;

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
		auto operator()(auto& env, callback_info& info, auto&& invocable) {
			auto run = [ & ]() -> decltype(auto) {
				return std::apply(
					std::forward<decltype(invocable)>(invocable),
					std::tuple_cat(
						std::forward_as_tuple(env),
						js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
					)
				);
			};
			auto run_with_result = [ & ]() -> decltype(auto) {
				if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
					run();
					return std::monostate{};
				} else {
					return run();
				}
			};
			return js::transfer_in_strict<napi_value>(run_with_result(), env);
		}
};

// Since napi runs each environment in its own thread we can store the environment data pointer here.
template <class Environment>
thread_local Environment* env_local = nullptr;

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
auto value<function_tag>::make(Environment& env, auto&& function) -> value<function_tag> {
	using function_type = std::decay_t<decltype(function.invocable)>;
	using signature_type = util::function_signature_t<function_type>;

	if constexpr (std::is_empty_v<function_type>) {
		// Constant expression function. `data` is the environment.
		static_assert(std::is_trivially_constructible_v<function_type>);
		auto callback = [](napi_env nenv, napi_callback_info info) -> napi_value {
			callback_info args{nenv, info};
			auto& env = *static_cast<Environment*>(args.data());
			invoke_callback<signature_type> invoke{};
			return invoke(env, args, function_type{});
		};
		return value<function_tag>::from(js::napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, &env));
	} else if constexpr (sizeof(function_type) == sizeof(void*) && std::is_trivially_destructible_v<function_type>) {
		// Trivial function type containing only a pointer size, which is probably function pointer.
		// `data` is the function and environment is stored in a thread local.
		env_local<Environment> = &env;
		auto callback = [](napi_env nenv, napi_callback_info info) -> napi_value {
			callback_info args{nenv, info};
			auto& env = *env_local<Environment>;
			invoke_callback<signature_type> invoke{};
			return invoke(env, args, std::bit_cast<function_type>(args.data()));
		};
		return value<function_tag>::from(js::napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, std::bit_cast<void*>(function.invocable)));
	} else {
		// Otherwise the function requires bound state. A holder is used which contains the environment
		// & function data.
		struct holder {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
				holder(Environment& env, function_type function) :
						env{&env},
						function{std::move(function)} {}
				Environment* env;
				function_type function;
				napi_ref reference;
		};
		// Callback definition
		auto callback = [](napi_env nenv, napi_callback_info info) -> napi_value {
			callback_info args{nenv, info};
			auto& holder_ref = *static_cast<holder*>(args.data());
			invoke_callback<signature_type> invoke{};
			return invoke(*holder_ref.env, args, holder_ref.function);
		};
		// Make function, stashed with the holder ref
		auto holder_ptr = std::make_unique<holder>(env, std::forward<decltype(function)>(function).invocable);
		auto js_function = js::napi::invoke(napi_create_function, napi_env{env}, function.name.data(), function.name.length(), callback, holder_ptr.get());
		// On destruction, clean up the function holder
		auto finalizer = [](napi_env /*env*/, void* finalize_data, void* /*finalize_hint*/) {
			auto* holder_ptr = static_cast<holder*>(finalize_data);
			js::napi::invoke0(napi_delete_reference, napi_env{*holder_ptr->env}, holder_ptr->reference);
			delete holder_ptr;
		};
		holder_ptr->reference =
			js::napi::invoke(napi_add_finalizer, napi_env{env}, js_function, holder_ptr.get(), finalizer, nullptr);
		holder_ptr.release();
		return value<function_tag>::from(js_function);
	}
}

} // namespace js::napi
