module;
#include <optional>
#include <type_traits>
#include <variant>
export module ivm.js:primitive.visit;
import :primitive.tag;
import :tag;
import :transfer;

namespace ivm::js {

// Tagged primitive types
template <class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct visit<void, Type> : visit<void, void> {
		using visit<void, void>::visit;
		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, std::forward<decltype(value)>(value));
		}
};

// `std::optional` visitor may yield `undefined`
template <class Meta, class Type>
struct visit<Meta, std::optional<Type>> : visit<Meta, Type> {
		using visit<Meta, Type>::visit;
		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value) {
				const visit<Meta, Type>& visit = *this;
				return visit(*std::forward<decltype(value)>(value), accept);
			} else {
				return accept(undefined_tag{}, std::monostate{});
			}
		}
};

} // namespace ivm::js
