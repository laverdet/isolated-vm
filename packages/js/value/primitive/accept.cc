module;
#include <concepts>
#include <cstdint>
#include <expected>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
export module isolated_js:primitive.accept;
import :bigint;
import :date;
import :transfer;
import ivm.utility;

namespace js {

// Static value acceptor
template <class Tag, auto Value>
struct accept_as_constant {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> decltype(Value) {
			return Value;
		}
};

// Lossless value acceptor
template <class Tag, class Type>
struct accept_without_narrowing {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> Type {
			return Type{std::forward<decltype(value)>(value)};
		}
};

// Generic coercing acceptor
template <template <class> class TagOf, class Canonical, class Type>
struct accept_coerced {
	private:
		// nb: For example, `number_of_tag<double>`
		using covariant_tag_type = TagOf<Type>;
		// nb: For example, `number_tag`
		using contravariant_tag_type = covariant_tag_type::tag_type;

	public:
		constexpr auto operator()(contravariant_tag_type /*tag*/, visit_holder /*visit*/, auto&& value) const -> Type {
			return coerce(util::type<Canonical>, std::forward<decltype(value)>(value));
		}

		template <class Subject>
		constexpr auto operator()(TagOf<Subject> /*tag*/, visit_holder /*visit*/, auto&& value) const -> Type {
			return coerce(util::type<Subject>, std::forward<decltype(value)>(value));
		}

		constexpr auto operator()(covariant_tag<covariant_tag_type> tag, visit_holder visit, auto&& value) const -> Type {
			return (*this)(*tag, visit, std::forward<decltype(value)>(value));
		}

	private:
		constexpr auto coerce(std::type_identity<Type> /*tag*/, auto&& value) const -> Type {
			return Type{std::forward<decltype(value)>(value)};
		}

		template <class Subject>
		constexpr auto coerce(std::type_identity<Subject> /*tag*/, auto&& value) const -> Type {
			auto subject = static_cast<Subject>(std::forward<decltype(value)>(value));
			auto result = static_cast<Type>(subject);
			if (static_cast<Subject>(result) != subject) {
				throw std::range_error{"Failed to losslessly coerce value"};
			}
			return result;
		}
};

// `undefined` & `null`
template <>
struct accept<void, std::monostate> : accept_as_constant<undefined_tag, std::monostate{}> {};

template <>
struct accept<void, std::nullptr_t> : accept_as_constant<null_tag, nullptr> {};

// `bool`
template <>
struct accept<void, bool> : accept_without_narrowing<boolean_tag, bool> {};

// `number` types
template <>
struct accept<void, double> : accept_coerced<number_tag_of, double, double> {};

template <>
struct accept<void, int32_t> : accept_coerced<number_tag_of, double, int32_t> {};

template <>
struct accept<void, uint32_t> : accept_coerced<number_tag_of, double, uint32_t> {};

// `bigint` types
template <>
struct accept<void, bigint> : accept_coerced<bigint_tag_of, bigint, bigint> {};

template <>
struct accept<void, int64_t> : accept_coerced<bigint_tag_of, bigint, int64_t> {};

template <>
struct accept<void, uint64_t> : accept_coerced<bigint_tag_of, bigint, uint64_t> {};

// `date`
template <>
struct accept<void, js_clock::time_point> : accept_without_narrowing<date_tag, js_clock::time_point> {};

// `string` types (no character set conversion is implemented)
template <class Char>
struct accept<void, std::basic_string<Char>> {
		using tag_type = string_tag_of<Char>;
		using value_type = std::basic_string<Char>;

		constexpr auto operator()(string_tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> value_type
			requires std::constructible_from<value_type, decltype(value)> {
			return value_type{std::forward<decltype(value)>(value)};
		}

		constexpr auto operator()(tag_type /*tag*/, visit_holder /*visit*/, auto&& value) const -> value_type {
			return value_type{std::forward<decltype(value)>(value)};
		}

		constexpr auto operator()(covariant_tag<tag_type> tag, const auto& visit, auto&& value) const -> value_type {
			return (*this)(*tag, visit, std::forward<decltype(value)>(value));
		}
};

// `std::optional` allows `undefined` in addition to the next acceptor
template <class Meta, class Type>
struct accept<Meta, std::optional<Type>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		using accept_type::operator();
		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> std::optional<Type> {
			return std::nullopt;
		}
};

// `std::expected` is abused to provide an undefined case similar to `std::optional`, but only when
// `undefined_in_tag` is passed. This detects missing object properties, as as opposed to
// `undefined` properties.
template <class Meta, class Type>
struct accept<Meta, std::expected<Type, undefined_in_tag>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using value_type = std::expected<Type, undefined_in_tag>;
		using accept_type::accept_type;

		constexpr auto operator()(auto_tag auto tag, const auto& visit, auto&& value) const -> value_type
			requires std::invocable<const accept_type&, decltype(tag), decltype(visit), decltype(value)> {
			return value_type{std::in_place, accept_type::operator()(tag, visit, std::forward<decltype(value)>(value))};
		}

		constexpr auto operator()(undefined_in_tag tag, visit_holder /*visit*/, const auto& /*value*/) const -> value_type {
			return value_type{std::unexpect, tag};
		}
};

} // namespace js
