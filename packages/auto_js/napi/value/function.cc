export module napi_js:function_definitions;
import :callback;
import std;

namespace js::napi {

template <class Result>
auto local_for_function::apply(const auto& lock, auto&& args) -> Result {
	auto argv = js::transfer_in_strict<std::vector<napi_value>>(std::forward<decltype(args)>(args), lock);
	return invoke<Result>(lock, std::span{argv});
}

template <class Result>
auto local_for_function::call(const auto& lock, auto&&... args) -> Result {
	auto argv = js::transfer_in_strict<std::array<napi_value, sizeof...(args)>>(std::forward_as_tuple(args...), lock);
	return invoke<Result>(lock, std::span{argv});
}

template <class Result>
auto local_for_function::invoke(const auto& lock, std::span<napi_value> args) -> Result {
	auto undefined = js::transfer_in_strict<napi_value>(std::monostate{}, lock);
	auto* result = napi::invoke(napi_call_function, napi_env{lock}, undefined, napi_value{*this}, args.size(), args.data());
	return js::transfer_out<Result>(result, lock);
}

template <class Environment>
auto local_for_function::make(const environment_lock_witness_of<Environment>& lock, auto function) -> local_of<function_tag> {
	auto [ callback, data ] = make_callback_storage(lock, make_free_function<Environment>(std::move(function).callback));
	auto make = [ & ](void* data) -> local_of<function_tag> {
		return local_of<function_tag>::from(napi::invoke(napi_create_function, napi_env{lock}, function.name.data(), function.name.length(), callback, data));
	};
	if constexpr (requires { typename decltype(data)::element_type; }) {
		// Function requires finalizer
		auto function = make(data.get());
		return apply_finalizer(std::move(data), [ & ](auto* data, napi_finalize finalize, void* hint) -> local_of<function_tag> {
			napi::invoke0(napi_add_finalizer, napi_env{lock}, function, data, finalize, hint, nullptr);
			return function;
		});
	} else {
		// No finalizer needed
		return make(data);
	}
}

} // namespace js::napi
