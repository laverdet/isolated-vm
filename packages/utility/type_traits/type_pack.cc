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
		explicit constexpr type_pack(Args... /*types*/)
			requires(sizeof...(Types) > 0 && (... && (type_of<Types>{} == Args{}))) {}

		template <class... Right>
		constexpr auto operator+(type_pack<Right...> /*right*/) const -> type_pack<Types..., Right...> {
			return {};
		}

		[[nodiscard]] constexpr auto size() const -> std::size_t {
			return sizeof...(Types);
		}
};

template <class... Type>
type_pack(Type...) -> type_pack<typename Type::type...>;

export template <std::size_t Index, class... Types>
constexpr auto get(type_pack<Types...> /*pack*/) -> util::type_of<Types... [ Index ]> {
	return {};
}

} // namespace util

namespace std {

// `util::type_pack` metaprogramming specializations
template <std::size_t Index, class... Types>
struct tuple_element<Index, util::type_pack<Types...>> {
		using type = util::type_of<Types...[ Index ]>;
};

template <class... Types>
struct tuple_size<util::type_pack<Types...>> : std::integral_constant<std::size_t, sizeof...(Types)> {};

} // namespace std
