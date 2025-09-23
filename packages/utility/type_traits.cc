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

// `std::type_identity<T>{}` alias
export template <class Type>
constexpr inline auto type = std::type_identity<Type>{};

// Capture a pack of types into a single type
export template <class... Type>
struct parameter_pack {
		parameter_pack() = default;
		explicit constexpr parameter_pack(std::type_identity<Type>... /*types*/)
			requires(sizeof...(Type) > 0) {}

		template <class... Right>
		constexpr auto operator+(parameter_pack<Right...> /*right*/) const {
			return parameter_pack<Type..., Right...>{};
		}

		constexpr auto size() const {
			return sizeof...(Type);
		}
};

template <class... Type>
parameter_pack(std::type_identity<Type>...) -> parameter_pack<Type...>;

export template <std::size_t Index, class... Types>
constexpr auto get(parameter_pack<Types...> /*pack*/) -> std::type_identity<Types... [ Index ]> {
	return {};
}

// Extract the `T` of a given `std::type_identity<T>`
export template <auto Type>
using meta_type_t = decltype(Type)::type;

// Return a sequence of index constants
export template <std::size_t Size>
consteval auto make_sequence() {
	// With C++26 P2686 we can do constexpr decomposition. So instead of the `tuple` of
	// `integral_constants` it can be an array of `size_t`.
	// https://clang.llvm.org/cxx_status.html

	// std::array<std::size_t, Size> result{};
	// std::ranges::copy(std::ranges::iota_view{std::size_t{0}, Size + 1}, result.data());
	// return result;

	constexpr auto make = []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{std::integral_constant<std::size_t, Index>{}...};
	};
	return make(std::make_index_sequence<Size>());
}

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

namespace std {

// `util::parameter_pack` metaprogramming specializations
template <std::size_t Index, class... Types>
struct tuple_element<Index, util::parameter_pack<Types...>> {
		using type = std::type_identity<Types...[ Index ]>;
};

template <class... Types>
struct tuple_size<util::parameter_pack<Types...>> : std::integral_constant<std::size_t, sizeof...(Types)> {};

} // namespace std
