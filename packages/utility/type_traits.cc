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
