module;
#include <functional>
export module ivm.utility:type_traits;

namespace util {

// Similar to `std::integral_constant` but with w/ c++17 auto type
export template <auto Value>
struct value_constant {
		using value_type = decltype(Value);
		constexpr auto operator*() const { return Value; }
		constexpr static auto value = Value;
};

// Capture a pack of types into a single type
export template <class... Type>
struct parameter_pack {};

// Select the indexed type from a parameter pack
export template <std::size_t Index, class... Types>
struct select;

export template <std::size_t Index, class... Types>
using select_t = select<Index, Types...>::type;

template <class Type, class... Types>
struct select<0, Type, Types...> : std::type_identity<Type> {};

template <std::size_t Index, class Type, class... Types>
struct select<Index, Type, Types...> : select<Index - 1, Types...> {};

// Copy the cv_ref qualifiers from `From` to `To`
export template <class From, class To>
struct apply_ref : std::type_identity<To> {};

export template <class From, class To>
using apply_ref_t = apply_ref<From, To>::type;

template <class From, class To>
struct apply_ref<From&, To> : std::type_identity<To&> {};

template <class From, class To>
struct apply_ref<From&&, To> : std::type_identity<To&&> {};

export template <class From, class To>
struct apply_cv : std::type_identity<To> {};

export template <class From, class To>
using apply_cv_t = apply_cv<From, To>::type;

template <class From, class To>
struct apply_cv<const From, To> : std::type_identity<const To> {};

template <class From, class To>
struct apply_cv<volatile From, To> : std::type_identity<volatile To> {};

template <class From, class To>
struct apply_cv<const volatile From, To> : std::type_identity<const volatile To> {};

export template <class From, class To>
struct apply_cv_ref
		: std::type_identity<apply_ref_t<From, apply_cv_t<std::remove_reference_t<From>, To>>> {};

export template <class From, class To>
using apply_cv_ref_t = apply_cv_ref<From, To>::type;

// Same as select, but counting from the end
export template <std::size_t Index, class... Types>
struct reverse_select : select<sizeof...(Types) - Index - 1, Types...> {};

export template <std::size_t Index, class... Types>
using reverse_select_t = reverse_select<Index, Types...>::type;

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
