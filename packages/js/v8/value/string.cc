module;
#include <cstdint>
#include <string>
#include <type_traits>
module v8_js.string;
import v8_js.handle;
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
	auto length = (*this)->Length();
	std::string string;
	string.resize_and_overwrite(length, [ this ](char* data, auto length) {
		auto* data_uint8 = reinterpret_cast<uint8_t*>(data);
		(*this)->WriteOneByte(isolate(), data_uint8, 0, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		return length;
	});
	return string;
}

auto string::materialize(std::type_identity<std::u16string> /*tag*/) const -> std::u16string {
	auto length = (*this)->Length();
	std::u16string string;
	string.resize_and_overwrite(length, [ this ](char16_t* data, auto length) {
		auto* data_uint16 = reinterpret_cast<uint16_t*>(data);
		(*this)->Write(isolate(), data_uint16, 0, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		return length;
	});
	return string;
}

} // namespace js::iv8
