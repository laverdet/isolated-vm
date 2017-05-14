#pragma once
#include <node.h>
#define THROW(x, m) return args.GetReturnValue().Set(args.GetIsolate()->ThrowException(x(v8::String::NewFromOneByte(args.GetIsolate(), (const uint8_t*)m, v8::NewStringType::kNormal).ToLocalChecked())))

namespace ivm {

inline v8::Local<v8::String> v8_string(const char* string) {
	return v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kNormal).ToLocalChecked();
}

inline v8::Local<v8::String> v8_symbol(const char* string) {
	return v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kInternalized).ToLocalChecked();
}

}
