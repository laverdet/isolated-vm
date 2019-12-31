#pragma once
#include <cassert>
#include <string>
#include <functional>
#include <v8.h>
#include "generic/error.h"

namespace ivm {

/**
 * Easy strings
 */
inline v8::Local<v8::String> v8_string(const char* string) {
	return Unmaybe(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kNormal)); // NOLINT
}

inline v8::Local<v8::String> v8_symbol(const char* string) {
	return Unmaybe(v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kInternalized)); // NOLINT
}

/**
 * Shorthand dereference of Persistent to Local
 */
template <typename T>
v8::Local<T> Deref(const v8::Persistent<T>& handle) {
	return v8::Local<T>::New(v8::Isolate::GetCurrent(), handle);
}

template <typename T>
v8::Local<T> Deref(const v8::Persistent<T, v8::CopyablePersistentTraits<T>>& handle) {
	return v8::Local<T>::New(v8::Isolate::GetCurrent(), handle);
}

}

#include "remote_handle.h"

namespace ivm {

template <typename T>
v8::Local<T> Deref(const RemoteHandle<T>& handle) {
	return handle.Deref();
}

} // namespace ivm
