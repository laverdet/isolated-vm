#pragma once
#include <cassert>
#include <string>
#include <functional>
#include <v8.h>
#include "./generic/error.h"

// TODO: Why is windows.h getting included all of a sudden????
#undef GetObject

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
 * Option checker
 */
inline bool IsOptionSet(const v8::Local<v8::Context>& context, const v8::Local<v8::Object>& options, const char* key) {
	return Unmaybe(options->Get(context, v8_string(key)))->ToBoolean(v8::Isolate::GetCurrent())->IsTrue();
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
