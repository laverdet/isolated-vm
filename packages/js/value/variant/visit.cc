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
		constexpr explicit visit(auto* transfer) :
				visit<Meta, Types>{transfer}... {}

		constexpr auto operator()(auto&& value, auto& accept) const -> accept_target_t<decltype(accept)> {
			using target_type = accept_target_t<decltype(accept)>;
			const auto visit_alternative =
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr -> target_type {
				using visit_type = visit<Meta, Types...[ Index ]>;
				return util::invoke_as<visit_type>(*this, std::get<Index>(std::forward<decltype(value)>(value)), accept);
			};
			return util::template_switch(
				value.index(),
				// nb: `util::sequence` is actually supposed to be a `std::array` but it's a tuple of
				// constant expressions right now instead. If P1789 passes then this would be
				// `std::index_sequence_for`.
				util::sequence<sizeof...(Types)>,
				util::overloaded{
					visit_alternative,
					[ & ]() -> target_type {
						std::unreachable();
						return visit_alternative(std::integral_constant<size_t, 0>{});
					},
				}
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

		template <class Accept>
		auto operator()(auto&& value, Accept& accept) const -> accept_target_t<Accept> {
			const auto visit_alternative =
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) {
					using alternative_type = substitute_recursive<Variant, Types...[ Index ]>;
					using visit_type = visit<Meta, alternative_type>;
					return util::invoke_as<visit_type>(*this, boost::get<alternative_type>(std::forward<decltype(value)>(value)), accept);
				};
			return util::template_switch(
				value.which(),
				util::sequence<sizeof...(Types)>,
				util::overloaded{
					visit_alternative,
					[ & ]() {
						std::unreachable();
						return visit_alternative(std::integral_constant<size_t, 0>{});
					},
				}
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
