module;
#include <concepts>
#include <exception>
#include <stdexcept>
#include <type_traits>
export module napi_js:api;
export import nodejs;
import ivm.utility;

namespace js::napi {

// Thrown from `invoke` to indicate that a napi operation failed and there is an exception waiting
// on the JS stack.
export class pending_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending napi error]"; }
};

// Catch `napi::pending_error` thrown by `napi::invoke`
constexpr auto invoke_napi_scope = [](auto implementation) -> napi_value {
	try {
		return implementation();
	} catch (const napi::pending_error& /*error*/) {
		return napi_value{};
	}
};

// Invoke the given napi function and throw if it fails
export template <class... Params>
auto invoke0(auto(function)(Params...)->napi_status, auto&&... args) -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	switch (function(std::forward<decltype(args)>(args)...)) {
		case napi_ok: return;
		case napi_pending_exception: throw napi::pending_error{};
		default: throw std::runtime_error{"napi dispatch error"};
	}
}

// Invoke the given napi function and terminate if it fails.
export template <class... Params>
auto invoke0_noexcept(auto(function)(Params...)->napi_status, auto&&... args) noexcept -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	if (function(std::forward<decltype(args)>(args)...) != napi_ok) {
		std::terminate();
	}
}

// Invokes the given napi function and returns the final "out" argument
template <class... Params>
using out_type = Params...[ sizeof...(Params) - 1 ];

export template <class... Params>
auto invoke(auto(function)(Params...)->napi_status, auto&&... args)
	requires std::invocable<decltype(function), decltype(args)..., out_type<Params...>> {
	using out_pointer_type = out_type<Params...>;
	static_assert(std::is_pointer_v<out_pointer_type>);
	std::remove_pointer_t<out_pointer_type> out_arg;
	js::napi::invoke0(function, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

export template <class... Params>
auto invoke_noexcept(auto(function)(Params...)->napi_status, auto&&... args) noexcept
	requires std::invocable<decltype(function), decltype(args)..., out_type<Params...>> {
	using out_pointer_type = out_type<Params...>;
	static_assert(std::is_pointer_v<out_pointer_type>);
	std::remove_pointer_t<out_pointer_type> out_arg;
	js::napi::invoke0_noexcept(function, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

} // namespace js::napi
