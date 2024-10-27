module;
#include <cstddef>
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits;

namespace ivm::util {

// Select the indexed type from a parameter pack
template <size_t Index, class... Types>
struct select;

export template <size_t Index, class... Types>
using select_t = select<Index, Types...>::type;

template <class Type, class... Types>
struct select<0, Type, Types...> : std::type_identity<Type> {};

template <size_t Index, class Type, class... Types>
struct select<Index, Type, Types...> : select<Index - 1, Types...> {};

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
	{ std::type_identity_t<To[]>{std::forward<From>(from)} }; // NOLINT(modernize-avoid-c-arrays)
};

template <class From, class To>
	requires std::is_convertible_v<From, To>
constexpr inline bool is_convertible_without_narrowing_v<From, To> =
	construct_without_narrowing<To, From>;

export template <class From, class To>
concept convertible_without_narrowing = is_convertible_without_narrowing_v<From, To>;

} // namespace ivm::util
