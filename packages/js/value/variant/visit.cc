module;
#include <boost/variant.hpp>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js.variant.visit;
import isolated_js.transfer;
import isolated_js.variant.types;
import ivm.utility;

namespace js {

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
					const auto& visitor = std::get<Index>(visitors);
					return visitor(std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}

	private:
		std::tuple<visit<Meta, Types>...> visitors;
};

// Visiting a `boost::variant` visits the underlying members
template <class Meta, class Variant>
struct visit_recursive_variant;

template <class Meta, class Variant, class... Types>
struct visit_recursive_variant<Meta, variant_of<Variant, Types...>> : visit<Meta, substitute_recursive<Variant, Types>>... {
		constexpr visit_recursive_variant(int dummy, const visit_root<Meta>& visit_) :
				visit<Meta, substitute_recursive<Variant, Types>>{dummy, visit_}... {}

		auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
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
struct visit<Meta, recursive_variant<First, Rest...>>
		: visit_recursive_variant<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		constexpr visit() :
				visit_recursive_variant<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>{0, *this} {}
};

} // namespace js
