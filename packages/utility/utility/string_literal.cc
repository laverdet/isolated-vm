module;
#include <algorithm>
#include <string_view>
export module ivm.utility:string_literal;
import :type_traits;

namespace util {

// Structural string literal which may be used as a template parameter. The string is
// null-terminated.
export template <size_t Size>
struct string_literal {
		// NOLINTNEXTLINE(google-explicit-constructor, modernize-avoid-c-arrays)
		consteval string_literal(const char (&string)[ Size ]) {
			std::copy_n(static_cast<const char*>(string), Size, payload.data());
		}

		// NOLINTNEXTLINE(google-explicit-constructor)
		consteval operator std::string_view() const { return {payload.data(), length()}; }
		consteval auto operator<=>(const string_literal& right) const -> std::strong_ordering { return payload <=> right.payload; }
		consteval auto operator==(const string_literal& right) const -> bool { return payload == right.payload; }
		constexpr auto operator==(std::string_view string) const -> bool { return std::string_view{*this} == string; }
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		[[nodiscard]] constexpr auto data() const -> const char (&)[ Size ] { return *reinterpret_cast<const char (*)[ Size ]>(payload.data()); }
		[[nodiscard]] consteval auto length() const -> size_t { return Size - 1; }

		std::array<char, Size> payload{};
};

namespace string_literals {

// "string literal value"_sl
export template <util::string_literal String>
constexpr auto operator""_sl() {
	return String;
}

// "string literal template"_st
export template <util::string_literal String>
constexpr auto operator""_st() -> util::value_constant<String> {
	return {};
}

} // namespace string_literals
} // namespace util
