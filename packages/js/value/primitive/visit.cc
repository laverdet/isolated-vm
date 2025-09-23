module;
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
export module isolated_js.primitive.visit;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Tagged primitive types
template <class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct visit<void, Type> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, std::forward<decltype(value)>(value));
		}
};

// Constant string visitors
template <>
struct visit<void, std::string_view> {
		constexpr auto operator()(std::string_view value, const auto& accept) const -> decltype(auto) {
			return accept(string_tag_of<char>{}, value);
		}
};

template <>
struct visit<void, const char*> {
		constexpr auto operator()(const char* value, const auto& accept) const -> decltype(auto) {
			return accept(string_tag_of<char>{}, std::string_view{value});
		}
};

// `util::string_literal` is a UTF-8 string of known size
template <size_t Size>
struct visit<void, util::string_literal<Size>> {
		constexpr auto operator()(const auto& value, const auto& accept) const -> decltype(auto) {
			return accept(string_tag_of<char>{}, value.data());
		}
};

// `std::optional` visitor may yield `undefined`
template <class Meta, class Type>
struct visit<Meta, std::optional<Type>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			if (value) {
				return visit_type::operator()(*std::forward<decltype(value)>(value), accept);
			} else {
				return accept(undefined_tag{}, std::monostate{});
			}
		}
};

} // namespace js
