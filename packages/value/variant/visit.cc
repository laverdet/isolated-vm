module;
#include <boost/variant.hpp>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.value:variant.visit;
import :transfer;
import :variant.types;
import ivm.utility;

namespace ivm::value {

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor elsewhere, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct visit<Meta, std::variant<Types...>> {
	public:
		constexpr visit() :
				visitors{visit<Meta, Types>{0, *this}...} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return util::visit_with_index(
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/, auto&& value) constexpr {
					const auto& visit = std::get<Index>(visitors);
					return visit(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}

	private:
		std::tuple<visit<Meta, Types>...> visitors;
};

// Visiting a `boost::variant` visits the underlying members
template <class Meta, class Variant, class... Types>
struct visit<Meta, variant_of<Variant, Types...>> : visit<Meta, substitute_recursive<Variant, Types>>... {
		constexpr visit(int dummy, const auto_visit auto& visit_) :
				visit<Meta, substitute_recursive<Variant, Types>>{dummy, visit_}... {}

		auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ]<class Value>(Value&& value) {
					const visit<Meta, std::decay_t<Value>>& visit = *this;
					return visit(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `visit` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct visit<Meta, recursive_variant<First, Rest...>>
		: visit<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		constexpr visit() :
				visit<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>{0, *this} {}
};

} // namespace ivm::value
