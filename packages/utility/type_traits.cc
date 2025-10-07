module;
#include <functional>
#include <type_traits>
export module ivm.utility:type_traits;

namespace util {

// Similar to `std::integral_constant` but with w/ c++17 auto type
export template <auto Value>
struct value_constant {
		using value_type = decltype(Value);
		constexpr auto operator*() const -> value_type { return Value; }
		constexpr static auto value = Value;
};

// Return a sequence of index constants
export template <std::size_t Size>
constexpr auto sequence = []() consteval {
	// With C++26 P2686 we can do constexpr decomposition. So instead of the `tuple` of
	// `integral_constants` it can be an array of `size_t`.
	// https://clang.llvm.org/cxx_status.html

	// std::array<std::size_t, Size> result{};
	// std::ranges::copy(std::ranges::iota_view{std::size_t{0}, Size}, result.begin());
	// return result;

	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{std::integral_constant<std::size_t, Index>{}...};
	}(std::make_index_sequence<Size>());
}();

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

// Non-explicit this member functions (abridged)
template <class Result, class Object, bool Ex, class... Args>
struct function_signature<auto (Object::*)(Args...) noexcept(Ex)->Result>
		: std::type_identity<auto(Object&, Args...)->Result> {};

template <class Result, class Object, bool Ex, class... Args>
struct function_signature<auto (Object::*)(Args...) && noexcept(Ex)->Result>
		: std::type_identity<auto(Object&&, Args...)->Result> {};

template <class Result, class Object, bool Ex, class... Args>
struct function_signature<auto (Object::*)(Args...) const noexcept(Ex)->Result>
		: std::type_identity<auto(const Object&, Args...)->Result> {};

// Implementation of `value_constant` for invocable values
export template <auto Invocable, class Signature = function_signature_t<decltype(Invocable)>>
struct invocable_constant;

template <class Result, class... Args, auto(Invocable)(Args...)->Result>
struct invocable_constant<Invocable, auto(Args...)->Result> : value_constant<Invocable> {
		constexpr auto operator()(Args... args) const -> decltype(auto) {
			return (**this)(std::forward<Args>(args)...);
		}
};

} // namespace util
