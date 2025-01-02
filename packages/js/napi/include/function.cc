module;
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
export module ivm.napi:function;
import :utility;
import :value;
import ivm.js;
import ivm.utility;
import napi;
import v8;

namespace ivm::js::napi {

template <>
struct factory<function_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		template <auto Fn>
		auto operator()(const free_function<Fn>& callback, auto& data) const -> value<function_tag>;
};

template <class Tuple>
struct tuple_pop_front;

template <class Type, class... Types>
struct tuple_pop_front<std::tuple<Type, Types...>>
		: std::type_identity<std::tuple<Types...>> {};

template <class Tuple>
using tuple_pop_front_t = typename tuple_pop_front<Tuple>::type;

template <auto Fn>
auto factory<function_tag>::operator()(const free_function<Fn>& /*callback*/, auto& data) const -> value<function_tag> {
	using parameters_type = tuple_pop_front_t<util::functor_parameters_t<decltype(Fn)>>;
	using data_type = std::remove_reference_t<decltype(data)>;
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
		return std::apply(
			Fn,
			std::tuple_cat(
				std::forward_as_tuple(data),
				// nb: Invokes `visit(napi_env)` constructor. The isolate is available on the environment
				// object without needing to invoke `v8::Isolate::GetCurrent()`. But that's in the nodejs
				// module so it would need to be further abstracted.
				js::transfer<parameters_type>(std::span{arguments}.subspan(0, count), std::tuple{env}, std::tuple{})
			)
		);
	};
	return value<function_tag>::from(js::napi::invoke(napi_create_function, env(), nullptr, 0, callback, &data));
}

} // namespace ivm::js::napi
