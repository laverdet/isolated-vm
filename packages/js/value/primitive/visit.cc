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

// `util::string_literal` is a UTF-8 string of known size
template <class Meta, size_t Size>
struct visit<Meta, util::string_literal<Size>> : visit<void, void> {
		using visit<void, void>::visit;
		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(string_tag_of<char>{}, value.data());
		}
};

// `std::optional` visitor may yield `undefined`
template <class Meta, class Type>
struct visit<Meta, std::optional<Type>> : visit<Meta, Type> {
		using visit<Meta, Type>::visit;
		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value) {
				return visit<Meta, Type>::operator()(*std::forward<decltype(value)>(value), accept);
			} else {
				return accept(undefined_tag{}, std::monostate{});
			}
		}
};

} // namespace ivm::js
