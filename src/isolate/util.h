#pragma once
#include <v8.h>
#include <cassert>
#include <string>
#include <functional>
#include "remote_handle.h"

namespace ivm {

template <typename T> v8::Local<T> Unmaybe(v8::MaybeLocal<T> handle);
template <typename T> T Unmaybe(v8::Maybe<T> handle);

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
 * JS + C++ exception, use with care
 */
class js_runtime_error : public std::exception {
};

class js_error_message : public js_runtime_error {
	private:
		const char* c_str;
		std::string cc_str;

	public:
		explicit js_error_message(const char* message) : c_str(message) {}
		explicit js_error_message(std::string message) : c_str(nullptr), cc_str(std::move(message)) {}

		const char* GetMessage() const {
			if (c_str == nullptr) {
				return cc_str.c_str();
			} else {
				return c_str;
			}
		}
};

class js_fatal_error : public js_error_message {
	public:
		explicit js_fatal_error(const char* message) : js_error_message(message) {}
		explicit js_fatal_error(std::string message) : js_error_message(std::move(message)) {}
};

class js_error_ctor_base : public js_error_message {
	public:
		explicit js_error_ctor_base(const char* message) : js_error_message(message) {}
		explicit js_error_ctor_base(std::string message) : js_error_message(std::move(message)) {}
		virtual v8::Local<v8::Value> ConstructError() const = 0;
};

template <v8::Local<v8::Value> (*F)(v8::Local<v8::String>)>
class js_error_ctor : public js_error_ctor_base {
	private:
		std::string stack_trace;
	public:
		explicit js_error_ctor(const char* message, std::string stack_trace) : js_error_ctor_base(message), stack_trace(std::move(stack_trace)) {}
		explicit js_error_ctor(std::string message) : js_error_ctor_base(std::move(message)) {}

		std::string GetStackTrace() const {
			return stack_trace;
		}

		v8::Local<v8::Value> ConstructError() const final {
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			v8::MaybeLocal<v8::String> maybe_message = v8::String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>(GetMessage()), v8::NewStringType::kNormal);
			v8::Local<v8::String> message_handle;
			if (maybe_message.ToLocal(&message_handle)) {
				v8::Local<v8::Object> error = F(message_handle).As<v8::Object>();
				if (!stack_trace.empty() && isolate->InContext()) {
					std::string stack_str = std::string(GetMessage()) + stack_trace;
					v8::MaybeLocal<v8::String> maybe_stack = v8::String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>(stack_str.c_str()), v8::NewStringType::kNormal);
					v8::MaybeLocal<v8::String> maybe_stack_symbol = v8::String::NewFromOneByte(isolate, reinterpret_cast<const uint8_t*>("stack"), v8::NewStringType::kNormal);
					v8::Local<v8::String> stack, stack_symbol;
					if (maybe_stack.ToLocal(&stack) && maybe_stack_symbol.ToLocal(&stack_symbol)) {
						error->Set(isolate->GetCurrentContext(), stack_symbol, stack).IsJust();
					}
				}
				return error;
			}
			// If the MaybeLocal is empty then I think v8 will have an exception on deck. I don't know if
			// there's any way to assert() this though.
			return {};
		}
};

using js_generic_error = js_error_ctor<v8::Exception::Error>;
using js_type_error = js_error_ctor<v8::Exception::TypeError>;
using js_range_error = js_error_ctor<v8::Exception::RangeError>;

/**
 * Convert a MaybeLocal<T> to Local<T> and throw an error if it's empty. Someone else should throw
 * the v8 exception.
 */
template <typename T>
v8::Local<T> Unmaybe(v8::MaybeLocal<T> handle) {
	v8::Local<T> local;
	if (handle.ToLocal(&local)) {
		return local;
	} else {
		throw js_runtime_error();
	}
}

template <typename T>
T Unmaybe(v8::Maybe<T> handle) {
	T just;
	if (handle.To(&just)) {
		return just;
	} else {
		throw js_runtime_error();
	}
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

template <typename T>
v8::Local<T> Deref(const RemoteHandle<T>& handle) {
	return handle.Deref();
}

} // namespace ivm
