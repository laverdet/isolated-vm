module;
#include <cstdint>
#include <string>
#include <type_traits>
module v8_js;
import :handle;
import v8;

namespace js::iv8 {

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

auto string::materialize(std::type_identity<std::string> /*tag*/) const -> std::string {
	std::string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char* data, auto length) {
		if (length > 0) {
			auto* data_uint8 = reinterpret_cast<uint8_t*>(data);
			(*this)->WriteOneByteV2(isolate(), 0, length, data_uint8, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

auto string::materialize(std::type_identity<std::u8string> /*tag*/) const -> std::u8string {
	std::u8string string;
	string.resize_and_overwrite((*this)->Utf8LengthV2(isolate()), [ this ](char8_t* data, auto length) {
		if (length > 0) {
			auto* data_char = reinterpret_cast<char*>(data);
			(*this)->WriteUtf8V2(isolate(), data_char, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

auto string::materialize(std::type_identity<std::u16string> /*tag*/) const -> std::u16string {
	std::u16string string;
	string.resize_and_overwrite((*this)->Length(), [ this ](char16_t* data, auto length) {
		if (length > 0) {
			auto* data_uint16 = reinterpret_cast<uint16_t*>(data);
			(*this)->WriteV2(isolate(), 0, length, data_uint16, v8::String::WriteOptions::NO_NULL_TERMINATION);
		}
		return length;
	});
	return string;
}

} // namespace js::iv8
