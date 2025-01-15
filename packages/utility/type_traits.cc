module;
#include <algorithm>
#include <string_view>
export module ivm.utility.type_traits;

namespace util {

// Select the indexed type from a parameter pack
template <size_t Index, class... Types>
struct select;

export template <size_t Index, class... Types>
using select_t = select<Index, Types...>::type;

template <class Type, class... Types>
struct select<0, Type, Types...> : std::type_identity<Type> {};

template <size_t Index, class Type, class... Types>
struct select<Index, Type, Types...> : select<Index - 1, Types...> {};

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

// Select the last element in a tuple
export template <class Tuple>
struct last_element;

template <class... Types>
struct last_element<std::tuple<Types...>> {
		using type = std::tuple_element_t<sizeof...(Types) - 1, std::tuple<Types...>>;
};

export template <class Tuple>
using last_element_t = typename last_element<Tuple>::type;

// Infer the parameters of a non-overloaded function for..
template <class Functor>
struct functor_parameters;

export template <class Functor>
using functor_parameters_t = functor_parameters<Functor>::type;

// ..lambda expression (default template instantiation)
template <class Functor>
struct functor_parameters
		: functor_parameters<decltype(+std::declval<Functor>())> {};

// ..function type
template <class Result, bool Noexcept, class... Params>
struct functor_parameters<Result(Params...) noexcept(Noexcept)>
		: std::type_identity<std::tuple<Params...>> {};

// ..function pointer
template <class Result, bool Noexcept, class... Params>
struct functor_parameters<Result (*)(Params...) noexcept(Noexcept)>
		: functor_parameters<Result(Params...) noexcept(Noexcept)> {};

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
