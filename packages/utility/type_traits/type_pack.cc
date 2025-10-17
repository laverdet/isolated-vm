module;
#include <tuple>
#include <type_traits>
export module ivm.utility:type_traits.type_pack;
import :type_traits.type_of;

namespace util {

// Capture a pack of types into a single type
export template <class... Types>
struct type_pack {
		using type = type_pack;
		type_pack() = default;

		template <class... Args>
		explicit consteval type_pack(Args... /*types*/)
			requires(sizeof...(Types) > 0 && (... && (type_of<Types>{}.operator==(Args{})))) {}

		// Watch out! The `+` operator performs ADL so uninstantiable types will fail
		consteval auto operator+(auto right) const { concat(right); }

		// Non-ADL version, which can also accept any number of arguments
		consteval auto concat() const -> type_pack { return {}; }

		template <class... Right>
		consteval auto concat(type_pack<Right...> /*right*/, auto&&... rest) const {
			return type_pack<Types..., Right...>{}.concat(rest...);
		}

		// Structured binding accessor
		template <std::size_t Index>
		[[nodiscard]] consteval auto get() const -> std::type_identity<Types... [ Index ]> { return {}; }
};

template <class... Type>
type_pack(Type...) -> type_pack<typename Type::type...>;

} // namespace util

namespace std {

// `util::type_pack` metaprogramming specializations
template <std::size_t Index, class... Types>
struct tuple_element<Index, util::type_pack<Types...>> {
		using type = std::type_identity<Types...[ Index ]>;
};

template <class... Types>
struct tuple_size<util::type_pack<Types...>> {
		constexpr static auto value = sizeof...(Types);
};

} // namespace std
