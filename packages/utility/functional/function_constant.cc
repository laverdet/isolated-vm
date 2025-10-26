module;
#include <utility>
export module ivm.utility:functional.function_constant;
export import :type_traits;

namespace util {

// Kind of related to P3774 [https://isocpp.org/files/papers/P3774R0.html] but returns an invocable
// which can be inferred with `function_signature_t`.
export template <auto Invocable>
struct function_constant;

export template <auto Invocable>
constexpr auto fn = function_constant<Invocable>{};

template <auto Invocable, class Function, class Signature>
struct function_constant_of;

// It requires `function_signature_t` but there's nothing saying that an overloaded invocable
// couldn't be passed. It would just be a different implementation.
template <auto Invocable>
struct function_constant : function_constant_of<Invocable, decltype(Invocable), function_signature_t<decltype(Invocable)>> {};

// Free function
template <auto Invocable, class Function, class... Args, bool Nx, class Result>
struct function_constant_of<Invocable, Function, auto(Args...) noexcept(Nx)->Result> {
		constexpr auto operator()(Args... args) const noexcept(Nx) -> Result {
			return Invocable(std::forward<Args>(args)...);
		}
};

// Member function pointer
template <auto Invocable, class Type, class Function, class That, class... Args, bool Nx, class Result>
struct function_constant_of<Invocable, Function Type::*, auto(That, Args...) noexcept(Nx)->Result> {
		constexpr auto operator()(That that, Args... args) noexcept(Nx) -> Result {
			return (std::forward<That>(that).*Invocable)(std::forward<Args>(args)...);
		}
};

} // namespace util
