module;
#include <cstdint>
#include <stdexcept>
#include <string_view>
module napi_js;
import :api;
import :bound_value;
import isolated_js;

namespace js::napi {

// undefined
auto value<undefined_tag>::make(const environment& env) -> value<undefined_tag> {
	return value<undefined_tag>::from(js::napi::invoke(napi_get_undefined, napi_env{env}));
}

// null
auto value<null_tag>::make(const environment& env) -> value<null_tag> {
	return value<null_tag>::from(js::napi::invoke(napi_get_null, napi_env{env}));
}

// boolean
auto value<boolean_tag>::make(const environment& env, bool boolean) -> value<boolean_tag> {
	return value<boolean_tag>::from(js::napi::invoke(napi_get_boolean, napi_env{env}, boolean));
}

bound_value<boolean_tag>::operator bool() const {
	return js::napi::invoke(napi_get_value_bool, env(), napi_value{*this});
}

// number
auto value<number_tag>::make(const environment& env, double number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_double, napi_env{env}, number));
}

auto value<number_tag>::make(const environment& env, int32_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_int32, napi_env{env}, number));
}

auto value<number_tag>::make(const environment& env, int64_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_int64, napi_env{env}, number));
}

auto value<number_tag>::make(const environment& env, uint32_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_uint32, napi_env{env}, number));
}

bound_value<number_tag>::operator double() const {
	return js::napi::invoke(napi_get_value_double, env(), napi_value{*this});
}

bound_value<number_tag>::operator int32_t() const {
	return js::napi::invoke(napi_get_value_int32, env(), napi_value{*this});
}

bound_value<number_tag>::operator int64_t() const {
	return js::napi::invoke(napi_get_value_int64, env(), napi_value{*this});
}

bound_value<number_tag>::operator uint32_t() const {
	return js::napi::invoke(napi_get_value_uint32, env(), napi_value{*this});
}

// bigint
auto value<bigint_tag>::make(const environment& env, const bigint& number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_words, napi_env{env}, number.sign_bit(), number.size(), number.data()));
}

auto value<bigint_tag>::make(const environment& env, int64_t number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_int64, napi_env{env}, number));
}

auto value<bigint_tag>::make(const environment& env, uint64_t number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_uint64, napi_env{env}, number));
}

bound_value<bigint_tag>::operator bigint() const {
	js::bigint value;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	size_t length;
	js::napi::invoke0(napi_get_value_bigint_words, env(), napi_value{*this}, &value.sign_bit(), &length, nullptr);
	value.resize_and_overwrite(length, [ & ](auto* words, auto length) {
		if (length > 0) {
			js::napi::invoke0(napi_get_value_bigint_words, env(), napi_value{*this}, &value.sign_bit(), &length, words);
		}
		return length;
	});
	return value;
}

bound_value<bigint_tag>::operator int64_t() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	int64_t value;
	return js::napi::invoke(napi_get_value_bigint_int64, env(), napi_value{*this}, &value);
}

bound_value<bigint_tag>::operator uint64_t() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	uint64_t value;
	return js::napi::invoke(napi_get_value_bigint_uint64, env(), napi_value{*this}, &value);
}

// string
auto value<string_tag>::make(const environment& env, std::string_view string) -> value<string_tag> {
	return value<string_tag_of<char>>::from(napi::invoke(napi_create_string_latin1, napi_env{env}, string.data(), string.length()));
}

auto value<string_tag>::make(const environment& env, std::u8string_view string) -> value<string_tag> {
	return value<string_tag_of<char8_t>>::from(napi::invoke(napi_create_string_utf8, napi_env{env}, reinterpret_cast<const char*>(string.data()), string.length()));
}

auto value<string_tag>::make(const environment& env, std::u16string_view string) -> value<string_tag> {
	return value<string_tag_of<char16_t>>::from(napi::invoke(napi_create_string_utf16, napi_env{env}, string.data(), string.length()));
}

auto value<string_tag>::make_property_name(const environment& env, std::string_view string) -> value<string_tag> {
	return value<string_tag_of<char>>::from(napi::invoke(node_api_create_property_key_latin1, napi_env{env}, reinterpret_cast<const char*>(string.data()), string.length()));
}

auto value<string_tag>::make_property_name(const environment& env, std::u8string_view string) -> value<string_tag> {
	return value<string_tag_of<char8_t>>::from(napi::invoke(node_api_create_property_key_utf8, napi_env{env}, reinterpret_cast<const char*>(string.data()), string.length()));
}

auto value<string_tag>::make_property_name(const environment& env, std::u16string_view string) -> value<string_tag> {
	return value<string_tag_of<char16_t>>::from(napi::invoke(node_api_create_property_key_utf16, napi_env{env}, string.data(), string.length()));
}

bound_value<string_tag>::operator std::string() const {
	std::string string;
	auto length = js::napi::invoke(napi_get_value_string_latin1, env(), napi_value{*this}, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char* data, size_t length) {
			js::napi::invoke(napi_get_value_string_latin1, env(), napi_value{*this}, data, length);
			return length - 1;
		});
	}
	return string;
}

bound_value<string_tag>::operator std::u16string() const {
	// nb: napi requires that the returned string is null-terminated for some reason.
	std::u16string string;
	auto length = js::napi::invoke(napi_get_value_string_utf16, env(), napi_value{*this}, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char16_t* data, size_t length) {
			js::napi::invoke(napi_get_value_string_utf16, env(), napi_value{*this}, data, length);
			return length - 1;
		});
	}
	return string;
}

} // namespace js::napi
