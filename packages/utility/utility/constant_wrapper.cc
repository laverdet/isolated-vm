
module;
#include <algorithm>
export module ivm.utility:constant_wrapper;

// Lifted from P2781R8:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2781r8.html

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay, modernize-avoid-c-arrays)
namespace util {

template <class Type>
struct fixed_value {
		using type = Type;
		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval fixed_value(type value) noexcept : value{value} {}
		type value;
};

template <class Type, std::size_t Extent>
struct fixed_value<Type[ Extent ]> {
		using type = Type[ Extent ];
		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval fixed_value(const type& init) noexcept { std::copy_n(init, Extent, value); }
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		type value;
};

template <class Type, std::size_t Extent>
fixed_value(const Type (&)[ Extent ]) -> fixed_value<Type[ Extent ]>;

export template <fixed_value Value>
struct constant_wrapper;

template <fixed_value Value>
struct constant_wrapper {
		using value_type = typename decltype(Value)::type;
		using type = constant_wrapper;

		constexpr static auto& value = Value.value;

		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval operator const value_type&() const noexcept { return value; }
};

export template <fixed_value Value>
constexpr auto cw = constant_wrapper<Value>{};

} // namespace util
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay, modernize-avoid-c-arrays)
