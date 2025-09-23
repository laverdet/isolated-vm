module;
#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js.primitive.accept;
import isolated_js.date;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Default acceptor just forwards the given value directly to the underlying type's constructor
template <class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<void, Type> {
		constexpr auto operator()(con_tag_for_t<Type> /*tag*/, auto&& value) const {
			if constexpr (util::is_convertible_without_narrowing_v<decltype(value), Type>) {
				return Type{std::forward<decltype(value)>(value)};
			} else {
				return static_cast<Type>(std::forward<decltype(value)>(value));
			}
		}
};

// `undefined` -> `std::monostate`
template <>
struct accept<void, std::monostate> {
		constexpr auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const {
			return std::monostate{};
		}
};

// `null` -> `std::nullptr_t`
template <>
struct accept<void, std::nullptr_t> {
		constexpr auto operator()(null_tag /*tag*/, const auto& /*null*/) const {
			return nullptr;
		}
};

// `std::optional` allows `undefined` in addition to the next acceptor
template <class Meta, class Type>
struct accept<Meta, std::optional<Type>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> std::optional<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value), decltype(rest)...> {
			return accept_type::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}

		constexpr auto operator()(undefined_tag /*tag*/, const auto& /*value*/) const -> std::optional<Type> {
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

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> value_type
			requires std::invocable<accept_type, decltype(tag), decltype(value), decltype(rest)...> {
			return value_type{
				std::in_place,
				accept_type::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...)
			};
		}

		constexpr auto operator()(undefined_in_tag tag, const auto& /*value*/) const -> value_type {
			return value_type{std::unexpect, tag};
		}
};

} // namespace js
