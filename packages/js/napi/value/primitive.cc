module;
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>
module napi_js.primitive;
import isolated_js;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js::napi {

// null & undefined
auto implementation<value_tag>::make(const environment& env, std::monostate /*undefined*/) -> value<undefined_tag> {
	return value<undefined_tag>::from(js::napi::invoke(napi_get_undefined, env));
}

auto implementation<value_tag>::make(const environment& env, std::nullptr_t /*null*/) -> value<null_tag> {
	return value<null_tag>::from(js::napi::invoke(napi_get_null, env));
}

auto implementation<undefined_tag>::make(const environment& env) -> value<undefined_tag> {
	return implementation<value_tag>::make(env, std::monostate{});
}

auto implementation<null_tag>::make(const environment& env) -> value<null_tag> {
	return implementation<value_tag>::make(env, nullptr);
}

// number
auto implementation<number_tag>::make(const environment& env, double number) -> value<number_tag_of<double>> {
	return value<number_tag_of<double>>::from(napi::invoke(napi_create_double, env, number));
}

auto implementation<number_tag>::make(const environment& env, int32_t number) -> value<number_tag_of<int32_t>> {
	return value<number_tag_of<int32_t>>::from(napi::invoke(napi_create_int32, env, number));
}

auto implementation<number_tag>::make(const environment& env, int64_t number) -> value<number_tag_of<int64_t>> {
	return value<number_tag_of<int64_t>>::from(napi::invoke(napi_create_int64, env, number));
}

auto implementation<number_tag>::make(const environment& env, uint32_t number) -> value<number_tag_of<uint32_t>> {
	return value<number_tag_of<uint32_t>>::from(napi::invoke(napi_create_uint32, env, number));
}

// bigint
auto implementation<bigint_tag>::make(const environment& env, int64_t number) -> value<bigint_tag_of<int64_t>> {
	return value<bigint_tag_of<int64_t>>::from(napi::invoke(napi_create_bigint_int64, env, number));
}

auto implementation<bigint_tag>::make(const environment& env, uint64_t number) -> value<bigint_tag_of<uint64_t>> {
	return value<bigint_tag_of<uint64_t>>::from(napi::invoke(napi_create_bigint_uint64, env, number));
}

// string
auto implementation<string_tag>::make(const environment& env, std::string_view string) -> value<string_tag_of<char>> {
	return value<string_tag_of<char>>::from(napi::invoke(napi_create_string_latin1, env, string.data(), string.length()));
}

auto implementation<string_tag>::make(const environment& env, std::u16string_view string) -> value<string_tag_of<char16_t>> {
	return value<string_tag_of<char16_t>>::from(napi::invoke(napi_create_string_utf16, env, string.data(), string.length()));
}

} // namespace js::napi
