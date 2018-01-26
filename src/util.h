#pragma once
#include <node.h>
#include <string>
#include <functional>

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
 * JS + C++ exception, use with care
 */
class js_error_base : public std::exception {};

template <v8::Local<v8::Value> (*F)(v8::Local<v8::String>)>
struct js_error : public js_error_base {
	explicit js_error(const std::string& message) {
		v8::Isolate* isolate = v8::Isolate::GetCurrent();
		const uint8_t* c_str = (const uint8_t*)message.c_str(); // NOLINT
		v8::MaybeLocal<v8::String> maybe_message = v8::String::NewFromOneByte(isolate, c_str, v8::NewStringType::kNormal);
		v8::Local<v8::String> message_handle;
		if (maybe_message.ToLocal(&message_handle)) {
			isolate->ThrowException(F(message_handle));
		}
		// If the MaybeLocal is empty then I think v8 will have an exception on deck. I don't know if
		// there's any way to assert() this though.
	}
};

using js_generic_error = js_error<v8::Exception::Error>;
using js_type_error = js_error<v8::Exception::TypeError>;

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
		throw js_error_base();
	}
}

template <typename T>
T Unmaybe(v8::Maybe<T> handle) {
	T just;
	if (handle.To(&just)) {
		return just;
	} else {
		throw js_error_base();
	}
}

/**
 * Run a function and annotate the exception with source / line number if it throws
 */
template <typename T, typename F>
T RunWithAnnotatedErrors(F&& fn) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::TryCatch try_catch(isolate);
	try {
		return fn();
	} catch (js_error_base& cc_error) {
		try {
			assert(try_catch.HasCaught());
			v8::Local<v8::Context> context = isolate->GetCurrentContext();
			v8::Local<v8::Value> error = try_catch.Exception();
			v8::Local<v8::Message> message = try_catch.Message();
			assert(error->IsObject());
			int linenum = Unmaybe(message->GetLineNumber(context));
			int start_column = Unmaybe(message->GetStartColumn(context));
			std::string decorator =
				std::string(*v8::String::Utf8Value(message->GetScriptResourceName())) +
				":" + std::to_string(linenum) +
				":" + std::to_string(start_column + 1);
			std::string message_str = *v8::String::Utf8Value(error.As<v8::Object>()->Get(v8_symbol("message")));
			error.As<v8::Object>()->Set(v8_symbol("message"), v8_string((message_str + " [" + decorator + "]").c_str()));
			isolate->ThrowException(error);
			throw js_error_base();
		} catch (js_error_base& cc_error) {
			try_catch.ReThrow();
			throw js_error_base();
		}
	}
}

} // namespace ivm
