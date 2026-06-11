module;
#include <v8/version.h>
module v8_js;
import :primitive;
import std;
import v8;

namespace js::iv8 {

// `value_for_boolean`
value_for_boolean::operator bool() const {
	return (*this)->Value();
}

// `value_for_number`
value_for_number::operator double() const {
	return (*this)->Value();
}

value_for_number::operator std::int32_t() const {
	return this->As<v8::Int32>()->Value();
}

// `value_for_bigint`
value_for_bigint::operator js::bigint() const {
	auto bigint = js::bigint{};
	bigint.resize_and_overwrite((*this)->WordCount(), [ & ](auto* words, auto length) {
		if (length > 0) {
			auto int_length = static_cast<int>(length);
			(*this)->ToWordsArray(&bigint.sign_bit(), &int_length, words);
		}
		return length;
	});
	return bigint;
}

value_for_bigint_i64::operator std::int64_t() const {
	return value_;
}

// `value_for_string`
value_for_string::operator std::string() const {
	std::string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char* data, std::size_t length) -> std::size_t {
		if (length > 0) {
			auto* data_uint8 = reinterpret_cast<std::uint8_t*>(data);
			(*this)->WriteOneByteV2(isolate(), 0, length, data_uint8, v8::String::WriteFlags::kNone);
		}
		return length;
	});
	return string;
}

value_for_string::operator std::u16string() const {
	std::u16string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char16_t* data, std::size_t length) -> std::size_t {
		if (length > 0) {
			auto* data_uint16 = reinterpret_cast<std::uint16_t*>(data);
			(*this)->WriteV2(isolate(), 0, length, data_uint16, v8::String::WriteFlags::kNone);
		}
		return length;
	});
	return string;
}

// `value_for_date`
value_for_date::operator js_clock::time_point() const {
	return js_clock::time_point{js_clock::duration{(*this)->ValueOf()}};
}

// `value_for_external`
value_for_external::operator void*() const {
#if V8_HAS_TAGGED_EXTERNAL
	// TODO: Use this feature
	return (*this)->Value(0);
#else
	return (*this)->Value();
#endif
}

} // namespace js::iv8
