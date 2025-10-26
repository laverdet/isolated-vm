module;
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits;
export import :type_traits.function_traits;
export import :type_traits.transform;
export import :type_traits.type_of;
export import :type_traits.type_pack;

namespace util {

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
