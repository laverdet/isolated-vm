module;
#include <cstdint>
#include <string>
#include <type_traits>
module ivm.v8;
import :handle;
import :string;
import :utility;
import v8;

namespace ivm::iv8 {

auto string::Cast(v8::Data* data) -> string* {
	return reinterpret_cast<string*>(v8::String::Cast(data));
}

auto string::make(v8::Isolate* isolate, const std::string& string) -> v8::Local<v8::String> {
	return make(isolate, std::string_view{string});
}

auto string::make(v8::Isolate* isolate, const std::u16string& string) -> v8::Local<v8::String> {
	return make(isolate, std::u16string_view{string});
}

auto string::make(v8::Isolate* isolate, std::string_view view) -> v8::Local<v8::String> {
	return unmaybe(v8::String::NewFromOneByte(
		isolate,
		reinterpret_cast<const uint8_t*>(view.data()),
		v8::NewStringType::kNormal,
		static_cast<int>(view.size())
	));
}

auto string::make(v8::Isolate* isolate, std::u16string_view view) -> v8::Local<v8::String> {
	return unmaybe(v8::String::NewFromTwoByte(
		isolate,
		reinterpret_cast<const uint16_t*>(view.data()),
		v8::NewStringType::kNormal,
		static_cast<int>(view.size())
	));
}

auto string::materialize(std::type_identity<std::string> /*tag*/, handle_env env) const -> std::string {
	auto length = Length();
	std::string string;
	string.resize_and_overwrite(length, [ this, &env ](char* data, auto length) {
		auto* data_uint8 = reinterpret_cast<uint8_t*>(data);
		WriteOneByte(env.isolate, data_uint8, 0, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		return length;
	});
	return string;
}

auto string::materialize(std::type_identity<std::u16string> /*tag*/, handle_env env) const -> std::u16string {
	auto length = Length();
	std::u16string string;
	string.resize_and_overwrite(length, [ this, &env ](char16_t* data, auto length) {
		auto* data_uint16 = reinterpret_cast<uint16_t*>(data);
		Write(env.isolate, data_uint16, 0, length, v8::String::WriteOptions::NO_NULL_TERMINATION);
		return length;
	});
	return string;
}

} // namespace ivm::iv8
