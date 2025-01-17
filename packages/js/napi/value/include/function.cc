module;
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
export module ivm.napi:function;
import :object_like;
import :utility;
import :value;
import isolated_js;
import ivm.utility;
import nodejs;
import v8;

namespace js::napi {

template <>
struct factory<function_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		template <auto Function, class Result, class Data, class... Args>
		auto operator()(const free_function<Function, Result(Data, Args...)>& callback, auto& data) const -> value<function_tag>;
};

template <auto Function, class Result, class Data, class... Args>
auto factory<function_tag>::operator()(const free_function<Function, Result(Data, Args...)>& /*function*/, auto& data) const -> value<function_tag> {
	using data_type = std::remove_reference_t<Data>;
	auto callback = [](napi_env env, napi_callback_info info) -> napi_value {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		std::array<napi_value, 8> arguments;
		auto count = arguments.size();
		// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
		void* data_ptr;
		js::napi::invoke0(napi_get_cb_info, env, info, &count, arguments.data(), nullptr, &data_ptr);
		if (count > arguments.size()) {
			throw std::runtime_error{"Too many arguments"};
		}
		auto& data = *static_cast<data_type*>(data_ptr);
		auto run = [ & ]() -> decltype(auto) {
			return std::apply(
				Function,
				std::tuple_cat(
					std::forward_as_tuple(data),
					// nb: Invokes `visit(napi_env)` constructor. The isolate is available on the environment
					// object without needing to invoke `v8::Isolate::GetCurrent()`. But that's in the nodejs
					// module so it would need to be further abstracted.
					js::transfer<std::tuple<Args...>>(std::span{arguments}.subspan(0, count), std::tuple{env}, std::tuple{})
				)
			);
		};
		if constexpr (std::is_void_v<std::invoke_result_t<decltype(run)>>) {
			run();
			return js::napi::value<undefined_tag>::make(env);
		} else {
			return js::transfer_strict<napi_value>(run(), std::tuple{}, std::tuple{env});
		}
	};
	return value<function_tag>::from(js::napi::invoke(napi_create_function, env(), nullptr, 0, callback, &data));
}

export class function : public object_like {
	public:
		function(napi_env env, value<js::function_tag> value) :
				object_like{env, value} {}

		template <class Result>
		auto invoke(auto&&... args) -> Result;
};

template <class Result>
auto function::invoke(auto&&... args) -> Result {
	std::array<napi_value, sizeof...(args)> arg_values{
		js::transfer_strict<napi_value>(args, std::tuple{}, std::tuple{env()})...
	};
	auto undefined = js::napi::value<js::undefined_tag>::make(env());
	auto* result = js::napi::invoke(napi_call_function, env(), undefined, *this, arg_values.size(), arg_values.data());
	return js::transfer<Result>(result, std::tuple{env()}, std::tuple{});
}

} // namespace js::napi
