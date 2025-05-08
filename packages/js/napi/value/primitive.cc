module;
#include <cstdint>
#include <stdexcept>
#include <string_view>
module napi_js.primitive;
import isolated_js;
import napi_js.utility;
import nodejs;

namespace js::napi {

// undefined
auto value<undefined_tag>::make(const environment& env) -> value<undefined_tag> {
	return value<undefined_tag>::from(js::napi::invoke(napi_get_undefined, env));
}

// null
auto value<null_tag>::make(const environment& env) -> value<null_tag> {
	return value<null_tag>::from(js::napi::invoke(napi_get_null, env));
}

// boolean
auto value<boolean_tag>::make(const environment& env, bool boolean) -> value<boolean_tag> {
	return value<boolean_tag>::from(js::napi::invoke(napi_get_boolean, env, boolean));
}

auto bound_value<boolean_tag>::materialize(std::type_identity<bool> /*tag*/) const -> bool {
	return js::napi::invoke(napi_get_value_bool, env(), *this);
}

// number
auto value<number_tag>::make(const environment& env, double number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_double, env, number));
}

auto value<number_tag>::make(const environment& env, int32_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_int32, env, number));
}

auto value<number_tag>::make(const environment& env, int64_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_int64, env, number));
}

auto value<number_tag>::make(const environment& env, uint32_t number) -> value<number_tag> {
	return value<number_tag>::from(napi::invoke(napi_create_uint32, env, number));
}

auto bound_value<number_tag>::materialize(std::type_identity<double> /*tag*/) const -> double {
	return js::napi::invoke(napi_get_value_double, env(), *this);
}

auto bound_value<number_tag>::materialize(std::type_identity<int32_t> /*tag*/) const -> int32_t {
	return js::napi::invoke(napi_get_value_int32, env(), *this);
}

auto bound_value<number_tag>::materialize(std::type_identity<int64_t> /*tag*/) const -> int64_t {
	return js::napi::invoke(napi_get_value_int64, env(), *this);
}

auto bound_value<number_tag>::materialize(std::type_identity<uint32_t> /*tag*/) const -> uint32_t {
	return js::napi::invoke(napi_get_value_uint32, env(), *this);
}

// bigint
auto value<bigint_tag>::make(const environment& env, const bigint& number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_words, env, number.sign_bit(), number.size(), number.data()));
}

auto value<bigint_tag>::make(const environment& env, int64_t number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_int64, env, number));
}

auto value<bigint_tag>::make(const environment& env, uint64_t number) -> value<bigint_tag> {
	return value<bigint_tag>::from(napi::invoke(napi_create_bigint_uint64, env, number));
}

auto bound_value<bigint_tag>::materialize(std::type_identity<bigint> /*tag*/) const -> bigint {
	js::bigint value;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	size_t length;
	js::napi::invoke0(napi_get_value_bigint_words, env(), *this, &value.sign_bit(), &length, nullptr);
	value.resize_and_overwrite(length, [ & ](auto* words, auto length) {
		if (length > 0) {
			js::napi::invoke0(napi_get_value_bigint_words, env(), *this, &value.sign_bit(), &length, words);
		}
		return length;
	});
	return value;
}

auto bound_value<bigint_tag>::materialize(std::type_identity<int64_t> /*tag*/) const -> int64_t {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	int64_t value;
	auto lossless = js::napi::invoke(napi_get_value_bigint_int64, env(), *this, &value);
	if (!lossless) {
		throw std::logic_error{"Bigint is too big"};
	}
	return value;
}

auto bound_value<bigint_tag>::materialize(std::type_identity<uint64_t> /*tag*/) const -> uint64_t {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	uint64_t value;
	auto lossless = js::napi::invoke(napi_get_value_bigint_uint64, env(), *this, &value);
	if (!lossless) {
		throw std::logic_error{"Bigint is too big"};
	}
	return value;
}

// string
auto value<string_tag>::make(const environment& env, std::u16string_view string) -> value<string_tag> {
	return value<string_tag>::from(napi::invoke(napi_create_string_utf16, env, string.data(), string.length()));
}

auto value<string_tag>::make(const environment& env, std::string_view string) -> value<string_tag> {
	return value<string_tag_of<char>>::from(napi::invoke(napi_create_string_latin1, env, string.data(), string.length()));
}

auto bound_value<string_tag>::materialize(std::type_identity<std::u16string> /*tag*/) const -> std::u16string {
	// nb: napi requires that the returned string is null-terminated for some reason.
	std::u16string string;
	auto length = js::napi::invoke(napi_get_value_string_utf16, env(), *this, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char16_t* data, size_t length) {
			js::napi::invoke(napi_get_value_string_utf16, env(), *this, data, length);
			return length - 1;
		});
	}
	return string;
}

auto bound_value<string_tag>::materialize(std::type_identity<std::string> /*tag*/) const -> std::string {
	std::string string;
	auto length = js::napi::invoke(napi_get_value_string_latin1, env(), *this, nullptr, 0);
	if (length > 0) {
		string.resize_and_overwrite(length + 1, [ this ](char* data, size_t length) {
			js::napi::invoke(napi_get_value_string_latin1, env(), *this, data, length);
			return length - 1;
		});
	}
	return string;
}

} // namespace js::napi
