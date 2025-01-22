module;
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>
export module napi_js.primitive;
import isolated_js;
import napi_js.environment;
import napi_js.value;
import napi_js.value.internal;
import nodejs;

namespace js::napi {

// Base specialization which provides a constructor & `null` / `undefined` value constructors
template <>
struct implementation<value_tag> : implementation<value_tag::tag_type> {
	public:
		static auto make(const environment& env, std::monostate /*undefined*/) -> value<undefined_tag>;
		static auto make(const environment& env, std::nullptr_t /*null*/) -> value<null_tag>;
};

// undefined & null
template <>
struct implementation<undefined_tag> : implementation<undefined_tag::tag_type> {
		using implementation<value_tag>::make;
		static auto make(const environment& env) -> value<undefined_tag>;
};

template <>
struct implementation<null_tag> : implementation<null_tag::tag_type> {
		using implementation<value_tag>::make;
		static auto make(const environment& env) -> value<null_tag>;
};

// number
template <>
struct implementation<number_tag> : implementation<number_tag::tag_type> {
		static auto make(const environment& env, double number) -> value<number_tag_of<double>>;
		static auto make(const environment& env, int32_t number) -> value<number_tag_of<int32_t>>;
		static auto make(const environment& env, int64_t number) -> value<number_tag_of<int64_t>>;
		static auto make(const environment& env, uint32_t number) -> value<number_tag_of<uint32_t>>;
};

// bigint
template <>
struct implementation<bigint_tag> : implementation<bigint_tag::tag_type> {
		static auto make(const environment& env, int64_t number) -> value<bigint_tag_of<int64_t>>;
		static auto make(const environment& env, uint64_t number) -> value<bigint_tag_of<uint64_t>>;
};

// string
template <>
struct implementation<string_tag> : implementation<string_tag::tag_type> {
		static auto make(const environment& env, std::string_view string) -> value<string_tag_of<char>>;
		static auto make(const environment& env, std::u16string_view string) -> value<string_tag_of<char16_t>>;

		template <size_t Size>
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		static auto make(const environment& env, const char string[ Size ]) -> value<string_tag_of<char>>;
};

// ---

template <size_t Size>
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
auto implementation<string_tag>::make(const environment& env, const char string[ Size ]) -> value<string_tag_of<char>> {
	return make(env, std::string_view{string, Size});
}
} // namespace js::napi
