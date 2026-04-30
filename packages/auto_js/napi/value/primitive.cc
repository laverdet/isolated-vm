module napi_js;
import :api;
import :bound_value;
import :value_handle;
import std;

namespace js::napi {

// boolean
bound_value_for_boolean::operator bool() const {
	return napi::invoke(napi_get_value_bool, env(), napi_value{*this});
}

// number
bound_value_for_number::operator double() const {
	return napi::invoke(napi_get_value_double, env(), napi_value{*this});
}

bound_value_for_number::operator std::int32_t() const {
	return napi::invoke(napi_get_value_int32, env(), napi_value{*this});
}

bound_value_for_number::operator std::int64_t() const {
	return napi::invoke(napi_get_value_int64, env(), napi_value{*this});
}

bound_value_for_number::operator std::uint32_t() const {
	return napi::invoke(napi_get_value_uint32, env(), napi_value{*this});
}

// bigint
bound_value_for_bigint::operator bigint() const {
	js::bigint value;
	auto one_word = std::uint64_t{};
	auto length = std::size_t{1};
	napi::invoke0(napi_get_value_bigint_words, env(), napi_value{*this}, &value.sign_bit(), &length, &one_word);
	if (value.sign_bit() == 0 && length == 1) {
		return js::bigint{one_word};
	}
	value.resize_and_overwrite(length, [ & ](auto* words, auto length) noexcept {
		if (length > 0) {
			napi::invoke0_noexcept(napi_get_value_bigint_words, env(), napi_value{*this}, &value.sign_bit(), &length, words);
		}
		return length;
	});
	return value;
}

bound_value_for_bigint::operator std::int64_t() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::int64_t value;
	// nb: `lossless` error not checked for consistency with `napi_get_value_int32`,
	// `napi_get_value_string_latin1`, etc which don't return any errors.
	napi::invoke(napi_get_value_bigint_int64, env(), napi_value{*this}, &value);
	return value;
}

bound_value_for_bigint::operator std::uint64_t() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::uint64_t value;
	napi::invoke(napi_get_value_bigint_uint64, env(), napi_value{*this}, &value);
	return value;
}

// string
auto value_for_string::make_property_name(const environment& env, std::string_view string) -> value<string_tag> {
	return value<string_tag>::from(napi::invoke(node_api_create_property_key_latin1, napi_env{env}, reinterpret_cast<const char*>(string.data()), string.length()));
}

auto value_for_string::make_property_name(const environment& env, std::u8string_view string) -> value<string_tag> {
	return value<string_tag>::from(napi::invoke(node_api_create_property_key_utf8, napi_env{env}, reinterpret_cast<const char*>(string.data()), string.length()));
}

auto value_for_string::make_property_name(const environment& env, std::u16string_view string) -> value<string_tag> {
	return value<string_tag>::from(napi::invoke(node_api_create_property_key_utf16, napi_env{env}, string.data(), string.length()));
}

bound_value_for_string::operator std::string() const {
	std::string string;
	auto length = napi::invoke(napi_get_value_string_latin1, env(), napi_value{*this}, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char* data, std::size_t length) noexcept -> std::size_t {
			napi::invoke_noexcept(napi_get_value_string_latin1, env(), napi_value{*this}, data, length);
			return length - 1;
		});
	}
	return string;
}

bound_value_for_string::operator std::u16string() const {
	// nb: napi requires that the returned string is null-terminated for some reason.
	std::u16string string;
	auto length = napi::invoke(napi_get_value_string_utf16, env(), napi_value{*this}, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char16_t* data, std::size_t length) noexcept -> std::size_t {
			napi::invoke_noexcept(napi_get_value_string_utf16, env(), napi_value{*this}, data, length);
			return length - 1;
		});
	}
	return string;
}

} // namespace js::napi
