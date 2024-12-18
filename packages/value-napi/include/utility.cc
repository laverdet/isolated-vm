module;
#include <cstring>
#include <stdexcept>
export module ivm.napi:utility;
import ivm.utility;
import napi;
import v8;

namespace ivm::napi {

// Convert a napi handle to a v8 handle
export auto to_v8(napi_value value) -> v8::Local<v8::Value> {
	v8::Local<v8::Value> local{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&local, static_cast<void*>(&value), sizeof(local));
	return local;
}

// Convert a v8 handle to a napi handle
export template <class Type>
auto from_v8(const v8::Local<Type>& local) -> napi_value {
	napi_value value{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&value, &local, sizeof(local));
	return value;
}

// Invoke the given napi function and throw if it fails
export auto invoke0(auto fn, auto&&... args) -> void {
	if (fn(std::forward<decltype(args)>(args)...) != napi_ok) {
		throw std::runtime_error("Dispatch failed");
	}
}

// Invokes the given napi function and returns the final "out" argument
export auto invoke(auto fn, auto&&... args) {
	using parameters_t = util::functor_parameters_t<decltype(fn)>;
	using out_arg_t = std::tuple_element_t<std::tuple_size_v<parameters_t> - 1, parameters_t>;
	static_assert(std::is_pointer_v<out_arg_t>);
	std::remove_pointer_t<out_arg_t> out_arg{};
	ivm::napi::invoke0(fn, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

} // namespace ivm::napi
