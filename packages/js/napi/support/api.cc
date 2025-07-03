module;
#include <concepts>
#include <exception>
#include <stdexcept>
#include <type_traits>
export module napi_js:api;
export import nodejs;
import ivm.utility;

namespace js::napi {

// Invoke the given napi function and throw if it fails
export template <class... Params>
auto invoke0(auto(function)(Params...)->napi_status, auto&&... args) -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	if (function(std::forward<decltype(args)>(args)...) != napi_ok) {
		throw std::runtime_error{"Dispatch failed"};
	}
}

// Invoke the given napi function and terminate if it fails. It's called `invoke_dtor` because you
// should invoke it instead of the other functions in destructors, unless you plan to catch.
export template <class... Params>
auto invoke_dtor(auto(function)(Params...)->napi_status, auto&&... args) noexcept -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	if (function(std::forward<decltype(args)>(args)...) != napi_ok) {
		std::terminate();
	}
}

// Invokes the given napi function and returns the final "out" argument
export template <class... Params>
auto invoke(auto(function)(Params...)->napi_status, auto&&... args)
	requires std::invocable<decltype(function), decltype(args)..., util::reverse_select_t<0, Params...>> {
	using out_type = util::reverse_select_t<0, Params...>;
	static_assert(std::is_pointer_v<out_type>);
	std::remove_pointer_t<out_type> out_arg;
	js::napi::invoke0(function, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

} // namespace js::napi
