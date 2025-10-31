module;
#include <utility>
#include <variant>
export module ivm.utility:variant;
import :type_traits.transform;
import :type_traits.type_of;
import :type_traits.type_pack;

namespace util {

// Apply a visitor to a variant and map its result to a new variant type. Repeated types are collapsed.
export auto map_variant(auto&& variant, auto visit) {
	constexpr auto result_from = [](auto alternative) {
		using param_type = util::apply_cvref_t<decltype(variant), type_t<alternative>>;
		using result_type = std::invoke_result_t<decltype(visit), param_type>;
		return type<std::remove_cvref_t<result_type>>;
	};
	const auto [... result_types ] = [ = ]<class... Types>(const std::variant<Types...>& /*variant*/) -> auto {
		return util::pack_unique(util::type_pack{result_from(type<Types>)...});
	}(variant);
	using result_type = std::variant<type_t<result_types>...>;
	return std::visit(
		[ & ](auto&& value) -> result_type {
			return result_type{visit(std::forward<decltype(value)>(value))};
		},
		std::forward<decltype(variant)>(variant)
	);
}

} // namespace util
