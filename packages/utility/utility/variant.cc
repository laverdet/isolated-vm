module;
#include <utility>
#include <variant>
export module util:utility.variant;
import :meta.algorithm;
import :type_traits;

namespace util {

// Apply a visitor to a variant and map its result to a new variant type. Repeated types are collapsed.
export auto map_variant(auto&& variant, auto visit) {
	constexpr auto result_from = [](auto alternative) {
		return type<decltype(visit)>(type<util::apply_cvref_t<decltype(variant), type_t<alternative>>>);
	};
	const auto [... alternative_types ] = util::make_type_pack(type<decltype(variant)>.remove_cvref());
	const auto [... result_types ] = util::pack_unique(result_from(alternative_types)...);
	using result_type = std::variant<type_t<result_types>...>;
	return std::visit(
		[ & ](auto&& value) -> result_type {
			return result_type{visit(std::forward<decltype(value)>(value))};
		},
		std::forward<decltype(variant)>(variant)
	);
}

} // namespace util
