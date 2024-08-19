module;
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits;

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

// `std::same_as` but with cv-ref removed
export template <class Type, class PlainType>
concept same_as_cvref = std::is_same_v<std::remove_cvref_t<Type>, PlainType>;

// `boost::noncopyable` actually prevents moving too
export class non_copyable {
	public:
		non_copyable() = default;
		non_copyable(const non_copyable&) = delete;
		non_copyable(non_copyable&&) = default;
		~non_copyable() = default;
		auto operator=(const non_copyable&) -> non_copyable& = delete;
		auto operator=(non_copyable&&) -> non_copyable& = default;
};

export class non_moveable {
	public:
		non_moveable() = default;
		non_moveable(const non_moveable&) = delete;
		non_moveable(non_moveable&&) = delete;
		~non_moveable() = default;
		auto operator=(const non_moveable&) -> non_moveable& = delete;
		auto operator=(non_moveable&&) -> non_moveable& = delete;
};
