module;
#include <concepts>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>
export module napi_js:api.invoke;
export import nodejs;
import util;

namespace js::napi {

// Thrown from `invoke` to indicate that a napi operation failed and there is an exception waiting
// on the JS stack.
export class pending_error : public std::exception {
	public:
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[pending napi error]"; }
};

// Invoke the given napi function, returning void, or throw if it fails
export template <class... Params>
auto invoke0(auto(function)(Params...)->napi_status, auto&&... args) -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	switch (function(std::forward<decltype(args)>(args)...)) {
		case napi_ok: return;
		case napi_pending_exception: throw napi::pending_error{};
		default: throw std::runtime_error{"napi dispatch error"};
	}
}

// Invoke the given napi function, returning a bool indicating success
export template <class... Params>
auto invoke0_maybe(auto(function)(Params...)->napi_status, auto&&... args) -> bool
	requires std::invocable<decltype(function), decltype(args)...> {
	switch (function(std::forward<decltype(args)>(args)...)) {
		case napi_ok: return true;
		case napi_pending_exception: return false;
		default: throw std::runtime_error{"napi dispatch error"};
	}
}

// Invoke the given napi function, returning void, or terminate if it fails.
export template <class... Params>
auto invoke0_noexcept(auto(function)(Params...)->napi_status, auto&&... args) noexcept -> void
	requires std::invocable<decltype(function), decltype(args)...> {
	if (function(std::forward<decltype(args)>(args)...) != napi_ok) {
		std::terminate();
	}
}

// Extract the `out` parameter type from a napi function signature
constexpr auto napi_invoke_out = [](auto... types) {
	constexpr auto take = util::overloaded{
		[]<class Param>(std::type_identity<Param> /*type*/) -> void {
			static_assert(false, "Last parameter is not a pointer");
		},
		[]<class Param>(std::type_identity<Param*> /*type*/) -> auto {
			return type<Param>;
		},
	};
	return take(types...[ sizeof...(types) - 1 ]);
};

template <class... Params>
using napi_invoke_out_t = type_t<napi_invoke_out(type<Params>...)>;

// Invoke the given napi function and throw if it fails.
export template <class... Params>
auto invoke(auto(function)(Params...)->napi_status, auto&&... args)
	requires std::invocable<decltype(function), decltype(args)..., napi_invoke_out_t<Params...>*> {
	napi_invoke_out_t<Params...> out_arg;
	napi::invoke0(function, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

// Invoke the given napi function and return `optional<T>` indicating failure.
export template <class... Params>
auto invoke_maybe(auto(function)(Params...)->napi_status, auto&&... args)
	requires std::invocable<decltype(function), decltype(args)..., napi_invoke_out_t<Params...>*> {
	using out_type = napi_invoke_out_t<Params...>;
	out_type out_arg;
	return napi::invoke0_maybe(function, std::forward<decltype(args)>(args)..., &out_arg)
		? std::optional<out_type>{std::move(out_arg)}
		: std::nullopt;
}

// Invoke the given napi function and terminate if it fails.
export template <class... Params>
auto invoke_noexcept(auto(function)(Params...)->napi_status, auto&&... args) noexcept
	requires std::invocable<decltype(function), decltype(args)..., napi_invoke_out_t<Params...>*> {
	napi_invoke_out_t<Params...> out_arg;
	napi::invoke0_noexcept(function, std::forward<decltype(args)>(args)..., &out_arg);
	return std::move(out_arg);
}

} // namespace js::napi
