module;
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>
export module napi_js.primitive;
import isolated_js;
import napi_js.environment;
import napi_js.value;
import nodejs;

namespace js::napi {

// Base specialization which provides a constructor & `null` / `undefined` value constructors
template <>
struct implementation<value_tag> {
	public:
		static auto make(const environment& env, std::monostate /*undefined*/) -> value<undefined_tag>;
		static auto make(const environment& env, std::nullptr_t /*null*/) -> value<null_tag>;
};

// undefined & null
template <>
struct implementation<undefined_tag> : implementation<value_tag> {
		using implementation<value_tag>::make;
		static auto make(const environment& env) -> value<undefined_tag>;
};

template <>
struct implementation<null_tag> : implementation<value_tag> {
		using implementation<value_tag>::make;
		static auto make(const environment& env) -> value<null_tag>;
};

// number
template <>
struct implementation<number_tag> : implementation<value_tag> {
		static auto make(const environment& env, double number) -> value<number_tag_of<double>>;
		static auto make(const environment& env, int32_t number) -> value<number_tag_of<int32_t>>;
		static auto make(const environment& env, int64_t number) -> value<number_tag_of<int64_t>>;
		static auto make(const environment& env, uint32_t number) -> value<number_tag_of<uint32_t>>;
};

template <>
struct implementation<number_tag_of<double>> : implementation<number_tag> {};

template <>
struct implementation<number_tag_of<int32_t>> : implementation<number_tag> {};

template <>
struct implementation<number_tag_of<int64_t>> : implementation<number_tag> {};

template <>
struct implementation<number_tag_of<uint32_t>> : implementation<number_tag> {};

// bigint
template <>
struct implementation<bigint_tag> : implementation<value_tag> {
		static auto make(const environment& env, int64_t number) -> value<bigint_tag_of<int64_t>>;
		static auto make(const environment& env, uint64_t number) -> value<bigint_tag_of<uint64_t>>;
};

template <>
struct implementation<bigint_tag_of<int64_t>> : implementation<bigint_tag> {};

template <>
struct implementation<bigint_tag_of<uint64_t>> : implementation<bigint_tag> {};

// string
template <>
struct implementation<string_tag> : implementation<value_tag> {
		static auto make(const environment& env, std::string_view string) -> value<string_tag_of<char>>;
		static auto make(const environment& env, std::u16string_view string) -> value<string_tag_of<char16_t>>;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(const environment& env, const char string[ Size ]) -> value<string_tag_of<char>>;
};

template <>
struct implementation<string_tag_of<char>> : implementation<string_tag> {};

template <>
struct implementation<string_tag_of<char16_t>> : implementation<string_tag> {};

// ---

template <size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
auto implementation<string_tag>::make(const environment& env, const char string[ Size ]) -> value<string_tag_of<char>> {
	return make(env, std::string_view{string, Size});
}
} // namespace js::napi
