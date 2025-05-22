module;
#include <cstddef>
#include <utility>
#include <variant>
export module ivm.utility:variant;

namespace util {
namespace detail {
// Like `std::visit` but it visits with a `std::integral_constant` of the variant index. That way
// you can use the index for other structures.
template <class... Types>
constexpr auto visit_with_index_(const auto& visitor, auto&& variant) -> decltype(auto) {
	auto next = []<size_t Index>(std::integral_constant<size_t, Index> index, const auto& next, const auto& visitor, auto&& variant) constexpr -> decltype(auto) {
		if constexpr (Index == sizeof...(Types)) {
			std::unreachable();
			return next(std::integral_constant<size_t, 0>{}, next, visitor, std::forward<decltype(variant)>(variant));
		} else if (Index == variant.index()) {
			return visitor(index, std::get<Index>(std::forward<decltype(variant)>(variant)));
		} else {
			return next(std::integral_constant<size_t, Index + 1>{}, next, visitor, std::forward<decltype(variant)>(variant));
		}
	};
	return next(std::integral_constant<size_t, 0>{}, next, visitor, std::forward<decltype(variant)>(variant));
}
} // namespace detail

export template <class... Types>
constexpr auto visit_with_index(const auto& visitor, const std::variant<Types...>& variant) -> decltype(auto) {
	return detail::visit_with_index_<Types...>(visitor, variant);
}

export template <class... Types>
constexpr auto visit_with_index(const auto& visitor, std::variant<Types...>&& variant) -> decltype(auto) {
	return detail::visit_with_index_<Types...>(visitor, std::move(variant));
}

} // namespace util
