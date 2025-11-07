module;
#include <cstdint>
#include <string>
module v8_js;
import :handle;
import :primitive;
import v8;

namespace js::iv8 {

// `boolean`
boolean::operator bool() const {
	return (*this)->Value();
}

// `number`
number::operator double() const {
	return (*this)->Value();
}

number::operator int32_t() const {
	return this->As<v8::Int32>()->Value();
}

number::operator int64_t() const {
	return this->As<v8::Integer>()->Value();
}

number::operator uint32_t() const {
	return this->As<v8::Uint32>()->Value();
}

// `bigint`
bigint::operator js::bigint() const {
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

bigint_u64::operator uint64_t() const {
	return value_;
}

// `string`
auto string::make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String> {
	auto string = v8::String::NewFromOneByte(
		isolate,
		reinterpret_cast<const uint8_t*>(view.data()),
		v8::NewStringType::kNormal,
		static_cast<int>(view.size())
	);
	return string.ToLocalChecked();
}

auto string::make(v8::Isolate* isolate, std::u8string_view view) -> v8::Local<v8::String> {
	auto string = v8::String::NewFromUtf8(
		isolate,
		reinterpret_cast<const char*>(view.data()),
		v8::NewStringType::kNormal,
		static_cast<int>(view.size())
	);
	return string.ToLocalChecked();
}

auto string::make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String> {
	auto string = v8::String::NewFromTwoByte(
		isolate,
		reinterpret_cast<const uint16_t*>(view.data()),
		v8::NewStringType::kNormal,
		static_cast<int>(view.size())
	);
	return string.ToLocalChecked();
}

string::operator std::string() const {
	std::string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char* data, size_t length) -> size_t {
		if (length > 0) {
			auto* data_uint8 = reinterpret_cast<uint8_t*>(data);
			(*this)->WriteOneByteV2(isolate(), 0, length, data_uint8, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

string::operator std::u8string() const {
	std::u8string string;
	string.resize_and_overwrite((*this)->Utf8LengthV2(isolate()), [ this ](char8_t* data, size_t length) -> size_t {
		if (length > 0) {
			auto* data_char = reinterpret_cast<char*>(data);
			(*this)->WriteUtf8V2(isolate(), data_char, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

string::operator std::u16string() const {
	std::u16string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char16_t* data, size_t length) -> size_t {
		if (length > 0) {
			auto* data_uint16 = reinterpret_cast<uint16_t*>(data);
			(*this)->WriteV2(isolate(), 0, length, data_uint16, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

// `date`
auto date::make(v8::Local<v8::Context> context, js_clock::time_point date) -> v8::Local<v8::Date> {
	return v8::Date::New(context, date.time_since_epoch().count()).ToLocalChecked().As<v8::Date>();
}

date::operator js_clock::time_point() const {
	return js_clock::time_point{js_clock::duration{(*this)->ValueOf()}};
}

// `external`
external::operator void*() const {
	return (*this)->Value();
}

} // namespace js::iv8
