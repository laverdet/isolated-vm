module;
#include <cstddef>
#include <optional>
#include <type_traits>
#include <variant>
export module ivm.js:primitive.accept;
import :date;
import :primitive.tag;
import :tag;
import :transfer;
import ivm.utility;

namespace ivm::js {

// Default acceptor just forwards the given value directly to the underlying type's constructor
template <class Meta, class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<Meta, Type> : accept<Meta, void> {
		using accept<Meta, void>::accept;
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
struct accept<void, std::monostate> : accept<void, void> {
		using accept<void, void>::accept;
		constexpr auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const {
			return std::monostate{};
		}
};

// `null` -> `std::nullptr_t`
template <>
struct accept<void, std::nullptr_t> : accept<void, void> {
		using accept<void, void>::accept;
		constexpr auto operator()(null_tag /*tag*/, const auto& /*null*/) const {
			return std::nullptr_t{};
		}
};

// `std::optional` allows `undefined` in addition to the next acceptor
template <class Meta, class Type>
struct accept<Meta, std::optional<Type>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept;

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> std::optional<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value), decltype(rest)...> {
			return {accept_type::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...)};
		}

		constexpr auto operator()(undefined_tag /*tag*/, const auto& /*value*/) const -> std::optional<Type> {
			return std::nullopt;
		}
};

} // namespace ivm::js
