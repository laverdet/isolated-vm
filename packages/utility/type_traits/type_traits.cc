module;
#include <functional>
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits;
export import :type_traits.transform;
export import :type_traits.type_of;
export import :type_traits.type_pack;

namespace util {

// Use `std::function` to deduce the signature of an invocable
export template <class Type>
struct function_signature;

export template <class Type>
using function_signature_t = function_signature<Type>::type;

template <class Type>
struct function_signature
		: function_signature<decltype(std::function{std::declval<Type>()})> {};

// Non-function type passed
template <class Type>
struct function_signature<std::function<Type>>;

// Most invocable types including: pointers, lambdas, function objects, explicit-this member functions
template <class Result, class... Args>
struct function_signature<std::function<auto(Args...)->Result>>
		: std::type_identity<auto(Args...)->Result> {};

// Similar to `std::integral_constant` but with w/ c++17 auto type
export template <auto Value>
struct value_constant {
		using value_type = decltype(Value);
		constexpr auto operator*() const -> value_type { return Value; }
		constexpr static auto value = Value;
};

// Implementation of `value_constant` for invocable values
export template <auto Invocable, class Signature = function_signature_t<decltype(Invocable)>>
struct invocable_constant;

template <class Result, class... Args, auto(Invocable)(Args...)->Result>
struct invocable_constant<Invocable, auto(Args...)->Result> : value_constant<Invocable> {
		constexpr auto operator()(Args... args) const -> decltype(auto) {
			return (**this)(std::forward<Args>(args)...);
		}
};

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0870r4.html
export template <class From, class To>
constexpr inline bool is_convertible_without_narrowing_v = false;

template <class To, class From>
concept construct_without_narrowing = requires(From&& from) {
	// NOLINTNEXTLINE(modernize-avoid-c-arrays)
	{ std::type_identity_t<To[]>{std::forward<From>(from)} };
};

template <class From, class To>
	requires std::is_convertible_v<From, To>
constexpr inline bool is_convertible_without_narrowing_v<From, To> =
	construct_without_narrowing<To, From>;

export template <class From, class To>
concept convertible_without_narrowing = is_convertible_without_narrowing_v<From, To>;

} // namespace util
