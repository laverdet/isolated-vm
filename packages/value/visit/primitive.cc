module;
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
export module ivm.value:primitive;
import :date;
import :tag;
import :visit;

namespace ivm::value {

// `undefined`
template <>
struct tag_for<std::monostate> : std::type_identity<undefined_tag> {};

// `null`
template <>
struct tag_for<std::nullptr_t> : std::type_identity<null_tag> {};

// `boolean`
template <>
struct tag_for<bool> : std::type_identity<boolean_tag> {};

// `number`
export using number_t = std::variant<int32_t, uint32_t, double>;

template <>
struct tag_for<int32_t> : std::type_identity<number_tag_of<int32_t>> {};

template <>
struct tag_for<uint32_t> : std::type_identity<number_tag_of<uint32_t>> {};

template <>
struct tag_for<double> : std::type_identity<number_tag_of<double>> {};

// `bigint`
export using bigint_t = std::variant<int64_t, uint64_t>;

template <>
struct tag_for<int64_t> : std::type_identity<number_tag_of<int64_t>> {};

template <>
struct tag_for<uint64_t> : std::type_identity<number_tag_of<uint64_t>> {};

// `date`
template <>
struct tag_for<js_clock::time_point> : std::type_identity<date_tag> {};

// `string`
export using string_t = std::variant<std::string, std::u16string>;

template <>
struct tag_for<std::string> : std::type_identity<string_tag_of<std::string>> {};

template <>
struct tag_for<std::string_view> : std::type_identity<string_tag_of<std::string_view>> {};

template <>
struct tag_for<std::u16string> : std::type_identity<string_tag_of<std::u16string>> {};

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
		constexpr auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const {
			return std::monostate{};
		}
};

// `null` -> `std::nullptr_t`
template <>
struct accept<void, std::nullptr_t> : accept<void, void> {
		using accept<void, void>::accept;
		constexpr auto operator()(null_tag /*tag*/, auto&& /*null*/) const {
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

		constexpr auto operator()(undefined_tag /*tag*/, auto&& /*value*/) const -> std::optional<Type> {
			return std::nullopt;
		}
};

// Tagged primitive types
template <class Type>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct visit<Type> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(tag_for_t<Type>{}, std::forward<decltype(value)>(value));
		}
};

// `std::optional` visitor may yield `undefined`
template <class Type>
struct visit<std::optional<Type>> : visit<Type> {
		using visit<Type>::visit;
		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			if (value) {
				const visit<Type>& visit = *this;
				return visit(*std::forward<decltype(value)>(value), accept);
			} else {
				return accept(undefined_tag{}, std::monostate{});
			}
		}
};

} // namespace ivm::value
