module;
#include <array>
#include <memory>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>
export module napi_js.function;
import isolated_js;
import ivm.utility;
import napi_js.environment;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8;

namespace js::napi {

template <>
class value<function_tag> : public detail::value_next<function_tag> {
	public:
		using detail::value_next<function_tag>::value_next;

		template <class Result>
		auto apply(auto_environment auto& env, auto&& args) -> Result;
		template <class Result>
		auto call(auto_environment auto& env, auto&&... args) -> Result;

		template <auto_environment Environment, class Invocable, class Result, class... Args>
		static auto make(Environment& env, js::bound_function<Invocable, Result(Environment&, Args...)> function) -> value<function_tag>;
		template <auto_environment Environment, auto Function, class Result, class... Args>
		static auto make(Environment& env, free_function<Function, Result(Environment&, Args...)> function) -> value<function_tag>;

	private:
		template <class Result>
		auto invoke(auto_environment auto& env, std::span<napi_value> args) -> Result;
};

// ---

struct callback_info : util::non_copyable {
	public:
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		callback_info(napi_env env, napi_callback_info info) {
			js::napi::invoke0(napi_get_cb_info, env, info, &count_, storage_.data(), nullptr, &data_);
			if (count_ > storage_.size()) {
				throw std::runtime_error{"Too many arguments"};
			}
		}

		[[nodiscard]] auto arguments() -> std::span<napi_value> { return std::span{storage_}.first(count_); }
		[[nodiscard]] auto data() const -> void* { return data_; }

	private:
		std::array<napi_value, 8> storage_;
		std::size_t count_{storage_.size()};
		void* data_;
};

template <class... Args>
auto invoke_callback(auto_environment auto& env, callback_info& info, const auto& invocable) -> napi_value {
	auto run = [ & ]() -> decltype(auto) {
		return std::apply(
			invocable,
			std::tuple_cat(
				std::forward_as_tuple(env),
				js::transfer_out<std::tuple<Args...>>(info.arguments(), env)
			)
		);
	};
	if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
		run();
		return js::napi::value<undefined_tag>::make(env);
	} else {
		return js::transfer_in_strict<napi_value>(run(), env);
	}
}

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
	auto* result = js::napi::invoke(napi_call_function, env, undefined, *this, args.size(), args.data());
	return js::transfer_out<Result>(result, env);
}

template <auto_environment Environment, class Invocable, class Result, class... Args>
auto value<function_tag>::make(Environment& env, js::bound_function<Invocable, Result(Environment&, Args...)> function) -> value<function_tag> {
	using function_type = js::bound_function<Invocable, Result(Environment&, Args...)>;
	// Holds environment + function data. I guess we could also use thread locals for the environment,
	// but we're already here so we might as well stash it in the function data.
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
		return invoke_callback<Args...>(*holder_ref.env, args, holder_ref.function);
	};
	// Make function, stashed with the holder ref
	auto holder_ptr = std::make_unique<holder>(env, std::move(function));
	auto js_function = js::napi::invoke(napi_create_function, env, nullptr, 0, callback, holder_ptr.get());
	// On destruction, clean up the function holder
	auto finalizer = [](napi_env /*env*/, void* finalize_data, void* /*finalize_hint*/) {
		auto* holder_ptr = static_cast<holder*>(finalize_data);
		js::napi::invoke0(napi_delete_reference, *holder_ptr->env, holder_ptr->reference);
		delete holder_ptr;
	};
	holder_ptr->reference =
		js::napi::invoke(napi_add_finalizer, env, js_function, holder_ptr.get(), finalizer, nullptr);
	holder_ptr.release();
	return value<function_tag>::from(js_function);
}

template <auto_environment Environment, auto Function, class Result, class... Args>
auto value<function_tag>::make(Environment& env, free_function<Function, Result(Environment&, Args...)> /*function*/) -> value<function_tag> {
	auto callback = [](napi_env nenv, napi_callback_info info) -> napi_value {
		callback_info args{nenv, info};
		auto& env = *static_cast<Environment*>(args.data());
		return invoke_callback<Args...>(env, args, Function);
	};
	return value<function_tag>::from(js::napi::invoke(napi_create_function, env, nullptr, 0, callback, &env));
}

} // namespace js::napi
