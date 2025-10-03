module;
#include <boost/variant.hpp>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js:variant.visit;
import :transfer;
import :variant.types;
import ivm.utility;

namespace js {

// `std::variant` visitor.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct visit<Meta, std::variant<Types...>> : visit<Meta, Types>... {
	public:
		constexpr explicit visit(auto* transfer) :
				visit<Meta, Types>{transfer}... {}

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return util::visit_with_index(
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/, auto&& value) constexpr {
					using variant_visit_type = visit<Meta, Types...[ Index ]>;
					return this->variant_visit_type::operator()(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// Visiting a `boost::variant` visits the underlying members
template <class Meta, class Variant>
struct visit_recursive_variant;

template <class Meta, class Variant, class... Types>
struct visit_recursive_variant<Meta, variant_of<Variant, Types...>> : visit<Meta, substitute_recursive<Variant, Types>>... {
		constexpr explicit visit_recursive_variant(auto* transfer) :
				visit<Meta, substitute_recursive<Variant, Types>>{transfer}... {}

		auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ]<class Value>(Value&& value) {
					const visit<Meta, std::decay_t<Value>>& visitor = *this;
					return visitor(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `visit` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
using visit_recursive_variant_type =
	visit_recursive_variant<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>;

template <class Meta, class First, class... Rest>
struct visit<Meta, recursive_variant<First, Rest...>> : visit_recursive_variant_type<Meta, First, Rest...> {
		using visit_recursive_variant_type<Meta, First, Rest...>::visit_recursive_variant_type;
};

} // namespace js
