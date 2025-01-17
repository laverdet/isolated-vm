module;
#include <algorithm>
#include <functional>
#include <string_view>
export module ivm.utility.type_traits;

namespace util {

// Select the indexed type from a parameter pack
export template <std::size_t Index, class... Types>
struct select;

export template <std::size_t Index, class... Types>
using select_t = select<Index, Types...>::type;

template <class Type, class... Types>
struct select<0, Type, Types...> : std::type_identity<Type> {};

template <std::size_t Index, class Type, class... Types>
struct select<Index, Type, Types...> : select<Index - 1, Types...> {};

// Same as select, but counting from the end
export template <std::size_t Index, class... Types>
struct reverse_select : select<sizeof...(Types) - Index - 1, Types...> {};

export template <std::size_t Index, class... Types>
using reverse_select_t = reverse_select<Index, Types...>::type;

// Structural string literal which may be used as a template parameter. The string is
// null-terminated, which is required by v8's `String::NewFromUtf8Literal`.
export template <size_t Size>
struct string_literal {
		// NOLINTNEXTLINE(google-explicit-constructor, modernize-avoid-c-arrays)
		consteval string_literal(const char (&string)[ Size ]) {
			std::copy_n(static_cast<const char*>(string), Size, payload.data());
		}

		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval operator std::string_view() const { return {payload.data(), length()}; }
		consteval auto operator<=>(const string_literal& right) const -> std::strong_ordering { return payload <=> right.payload; }
		consteval auto operator==(const string_literal& right) const -> bool { return payload == right.payload; }
		constexpr auto operator==(std::string_view string) const -> bool { return std::string_view{*this} == string; }
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		[[nodiscard]] constexpr auto data() const -> const char (&)[ Size ] { return *reinterpret_cast<const char(*)[ Size ]>(payload.data()); }
		[[nodiscard]] consteval auto length() const -> size_t { return Size - 1; }

		std::array<char, Size> payload{};
};

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

// Free function
template <class Result, class... Args>
struct function_signature<std::function<auto(Args...)->Result>>
		: std::type_identity<auto(Args...)->Result> {};

// Pointer type intentionally erased because it is pointless
template <class Result, class... Args>
struct function_signature<std::function<auto (*)(Args...)->Result>>
		: std::type_identity<auto(Args...)->Result> {};

// Member function (this doesn't seem to actually work)
template <class Result, class Object, class... Args>
struct function_signature<std::function<auto (Object::*)(Args...)->Result>>
		: std::type_identity<auto (Object::*)(Args...)->Result> {};

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
