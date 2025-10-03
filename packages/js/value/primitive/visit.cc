module;
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
export module isolated_js:primitive.visit;
import :bigint;
import :date;
import :transfer;
import ivm.utility;

namespace js {

// The most basic of visitors which passes along a value with the tag supplied in the template.
template <class Tag>
struct visit_value_tagged {
		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, Tag{}, *this, std::forward<decltype(value)>(value));
		}
};

// `undefined` & `null`
template <>
struct visit<void, std::monostate> : visit_value_tagged<undefined_tag> {};

template <>
struct visit<void, std::nullptr_t> : visit_value_tagged<null_tag> {};

// `boolean`
template <>
struct visit<void, bool> : visit_value_tagged<boolean_tag> {};

// `number`
template <>
struct visit<void, double> : visit_value_tagged<number_tag_of<double>> {};

template <>
struct visit<void, int32_t> : visit_value_tagged<number_tag_of<int32_t>> {};

template <>
struct visit<void, uint32_t> : visit_value_tagged<number_tag_of<uint32_t>> {};

// `bigint`
template <>
struct visit<void, bigint> : visit_value_tagged<bigint_tag_of<bigint>> {};

template <>
struct visit<void, int64_t> : visit_value_tagged<bigint_tag_of<int64_t>> {};

template <>
struct visit<void, uint64_t> : visit_value_tagged<bigint_tag_of<uint64_t>> {};

// `date`
template <>
struct visit<void, js_clock::time_point> : visit_value_tagged<date_tag> {};

// `string`
template <class Char>
struct visit<void, std::basic_string<Char>> : visit_value_tagged<string_tag_of<Char>> {};

template <class Char>
struct visit<void, std::basic_string_view<Char>> : visit_value_tagged<string_tag_of<Char>> {};

// Constant string visitor
template <>
struct visit<void, const char*> {
		constexpr auto operator()(const char* value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, string_tag_of<char>{}, *this, std::string_view{value});
		}
};

// `util::string_literal` is a latin1 string of known size
template <size_t Size>
struct visit<void, util::string_literal<Size>> {
		constexpr auto operator()(const auto& value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, string_tag_of<char>{}, *this, value.data());
		}
};

// `std::optional` visitor may yield `undefined`
template <class Meta, class Type>
struct visit<Meta, std::optional<Type>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			if (value) {
				return visit_type::operator()(*std::forward<decltype(value)>(value), accept);
			} else {
				return invoke_accept(accept, undefined_tag{}, *this, std::monostate{});
			}
		}
};

} // namespace js
