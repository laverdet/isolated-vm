module;
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>
export module auto_js:variant.visit;
import :transfer;
import :variant.types;
import util;

namespace js {

// `std::variant` visitor.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct visit<Meta, std::variant<Types...>> : visit<Meta, Types>... {
		constexpr explicit visit(auto* transfer) :
				visit<Meta, Types>{transfer}... {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			using target_type = accept_target_t<Accept>;
			const auto visit_alternative =
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr -> target_type {
				using visit_type = visit<Meta, Types...[ Index ]>;
				return util::invoke_as<visit_type>(*this, std::get<Index>(std::forward<decltype(subject)>(subject)), accept);
			};
			return util::template_switch(
				subject.index(),
				// nb: `util::sequence` is actually supposed to be a `std::array` but it's a tuple of
				// constant expressions right now instead. If P1789 passes then this would be
				// `std::index_sequence_for`.
				util::sequence<sizeof...(Types)>,
				util::overloaded{
					visit_alternative,
					[ & ]() -> target_type { std::unreachable(); },
				}
			);
		}
};

} // namespace js
