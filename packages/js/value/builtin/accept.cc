module;
#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <variant>
export module isolated_js:builtin.accept;
import :bigint;
import :date;
import :external;
import :external;
import :error;
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
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
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
		constexpr auto operator()(contravariant_tag_type /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return coerce(std::type_identity<Canonical>{}, std::forward<decltype(subject)>(subject));
		}

		template <class Subject>
		constexpr auto operator()(TagOf<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> Type {
			return coerce(std::type_identity<Subject>{}, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(covariant_tag<covariant_tag_type> tag, visit_holder visit, auto&& subject) const -> Type {
			return (*this)(*tag, visit, std::forward<decltype(subject)>(subject));
		}

	private:
		[[nodiscard]] constexpr auto coerce(std::type_identity<Type> /*tag*/, auto&& subject) const -> Type {
			return Type{std::forward<decltype(subject)>(subject)};
		}

		template <class Subject>
		[[nodiscard]] constexpr auto coerce(std::type_identity<Subject> /*tag*/, auto&& subject) const -> Type {
			auto coerced = static_cast<Subject>(std::forward<decltype(subject)>(subject));
			auto result = static_cast<Type>(coerced);
			if (static_cast<Subject>(result) != coerced) {
				throw js::range_error{u"Could not coerce value"};
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

// `string` types (no utf8 interpolation is implemented)
template <class Char>
struct accept_coerced_string {
	private:
		using value_type = std::basic_string<Char>;

	public:
		constexpr auto operator()(string_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return coerce(std::type_identity<char16_t>{}, std::forward<decltype(subject)>(subject));
		}

		template <class Subject>
		constexpr auto operator()(string_tag_of<Subject> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> value_type {
			return coerce(std::type_identity<Subject>{}, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(covariant_tag<string_tag_of<Char>> tag, visit_holder visit, auto&& subject) const -> value_type {
			return (*this)(*tag, visit, std::forward<decltype(subject)>(subject));
		}

	private:
		[[nodiscard]] constexpr auto coerce(std::type_identity<Char> /*tag*/, auto&& subject) const -> value_type {
			return value_type{std::forward<decltype(subject)>(subject)};
		}

		template <class Subject>
		[[nodiscard]] constexpr auto coerce(std::type_identity<Subject> /*tag*/, auto&& subject) const -> value_type {
			constexpr auto max_char = util::overloaded{
				// latin1
				[](std::type_identity<char>) constexpr -> int { return 127; },
				// utf16
				[](std::type_identity<char16_t>) constexpr -> int { return 65'535; },
			}(type<Char>);

			// Check string range (since `resize_and_overwrite` may not throw)
			auto from = std::basic_string<Subject>{std::forward<decltype(subject)>(subject)};
			auto size = from.size();
			auto out_of_range = std::ranges::any_of(from, [](Subject ch) -> bool { return ch > max_char; });
			if (out_of_range) {
				throw js::range_error{u"String character out of range"};
			}

			// Copy coerced characters
			value_type result;
			result.resize_and_overwrite(size, [ & ](Char* data, size_t size) noexcept -> size_t {
				for (std::size_t ii = 0; ii < size; ++ii) {
					// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					data[ ii ] = static_cast<Subject>(from[ ii ]);
				}
				return size;
			});
			return result;
		}
};

template <class Char>
struct accept<void, std::basic_string<Char>> : accept_coerced_string<Char> {};

// `tagged_external` acceptors
template <>
struct accept<void, tagged_external&> {
		constexpr auto operator()(object_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> tagged_external& {
			if (subject.contains(std::type_identity<tagged_external>{})) {
				auto* pointer = static_cast<void*>(subject);
				return *static_cast<tagged_external*>(pointer);
			} else {
				throw js::type_error{u"Invalid object type"};
			}
		}
};

template <class Type>
struct accept<void, tagged_external_of<Type>&> : accept<void, tagged_external&> {
		using accept_type = accept<void, tagged_external&>;
		constexpr auto operator()(object_tag tag, visit_holder visit, auto subject) const -> tagged_external_of<Type>& {
			tagged_external& external = util::invoke_as<accept_type>(*this, tag, visit, std::move(subject));
			if (external.contains(std::type_identity<Type>{})) {
				return static_cast<tagged_external_of<Type>&>(external);
			} else {
				throw js::type_error{u"Invalid object type"};
			}
		}
};

template <class Type>
	requires requires { typename transfer_type_t<Type>; }
struct accept<void, Type&> : accept<void, transfer_type_t<Type>&> {
		using accept<void, transfer_type_t<Type>&>::accept;
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

// Accepting a pointer uses the reference acceptor, while also accepting `undefined`
template <class Meta, class Type>
struct accept<Meta, Type*> : accept<Meta, Type&> {
		using accept_type = accept<Meta, Type&>;
		using accept_type::accept_type;

		using accept_type::operator();

		constexpr auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*value*/) const -> Type* {
			return nullptr;
		}

		constexpr auto operator()(Type& target) const -> Type* {
			return std::addressof(target);
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

		using accept_type::operator();

		constexpr auto operator()(undefined_in_tag tag, visit_holder /*visit*/, const auto& /*value*/) const -> value_type {
			return value_type{std::unexpect, tag};
		}

		constexpr auto operator()(Type&& target) const -> value_type {
			return value_type{std::in_place, std::move(target)};
		}
};

} // namespace js
