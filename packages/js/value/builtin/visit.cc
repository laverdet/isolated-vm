module;
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
export module isolated_js:builtin.visit;
import :bigint;
import :date;
import :error;
import :transfer;
import ivm.utility;

namespace js {

// `util::cw` visitor
template <auto Value>
struct visit<void, util::constant_wrapper<Value>> : visit<void, std::remove_cv_t<typename util::constant_wrapper<Value>::value_type>> {
		using value_type = std::remove_cvref_t<typename util::constant_wrapper<Value>::value_type>;
		using visit_type = visit<void, value_type>;

		template <class Accept>
		constexpr auto operator()(const auto& subject, const Accept& accept) const -> accept_target_t<Accept> {
			const value_type& subject_value = subject;
			return util::invoke_as<visit_type>(*this, subject_value, accept);
		}
};

// The most basic of visitors which passes along a value with the tag supplied in the template.
template <class Tag>
struct visit_value_tagged {
		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(Tag{}, *this, std::forward<decltype(subject)>(subject));
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
template <std::integral Char, size_t Extent>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
struct visit<void, Char[ Extent ]> {
		template <class Accept>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		constexpr auto operator()(const Char (&subject)[ Extent ], const Accept& accept) const -> accept_target_t<Accept> {
			return accept(string_tag_of<Char>{}, *this, std::basic_string_view<Char>{subject, Extent - 1});
		}
};

// `Error` types
template <>
struct visit<void, js::error_value> {
		template <class Accept>
		constexpr auto operator()(const js::error_value& subject, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(error_tag{}, *this, subject);
		}
};

template <>
struct visit<void, js::error> : visit<void, js::error_value> {
		template <class Accept>
		constexpr auto operator()(const js::error& subject, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(error_tag{}, *this, js::error_value{subject});
		}
};

// `std::optional` visitor may yield `undefined`
template <class Meta, class Type>
struct visit<Meta, std::optional<Type>> : visit<Meta, Type> {
		using visit_type = visit<Meta, Type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject) {
				return util::invoke_as<visit_type>(*this, *std::forward<decltype(subject)>(subject), accept);
			} else {
				return accept(undefined_tag{}, *this, std::monostate{});
			}
		}
};

} // namespace js
