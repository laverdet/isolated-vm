#pragma once
#include <node.h>
#include <string>
#include <functional>

#define THROW(x, m) return args.GetReturnValue().Set(args.GetIsolate()->ThrowException(x(v8::String::NewFromOneByte(args.GetIsolate(), (const uint8_t*)m, v8::NewStringType::kNormal).ToLocalChecked())))

namespace ivm {

/**
 * Easy strings
 */
inline v8::Local<v8::String> v8_string(const char* string) {
	return v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kNormal).ToLocalChecked();
}

inline v8::Local<v8::String> v8_symbol(const char* string) {
	return v8::String::NewFromOneByte(v8::Isolate::GetCurrent(), (const uint8_t*)string, v8::NewStringType::kInternalized).ToLocalChecked();
}

/**
 * JS + C++ exception, use with care
 */
class js_error_base : public std::exception {};

template <v8::Local<v8::Value> (*F)(v8::Local<v8::String>)>
struct js_error : public js_error_base {
	js_error(std::string message) {
		v8::Isolate::GetCurrent()->ThrowException(F(v8_string(message.c_str())));
	}
};

typedef js_error<v8::Exception::Error> js_generic_error;
typedef js_error<v8::Exception::TypeError> js_type_error;

/**
 * Convert a MaybeLocal<T> to Local<T> and throw an error if it's empty. Someone else should throw
 * the v8 exception.
 */
template <typename T>
v8::Local<T> Unmaybe(v8::MaybeLocal<T> handle) {
	if (handle.IsEmpty()) {
		throw js_error_base();
	} else {
		return handle.ToLocalChecked();
	}
}

template <typename T>
T Unmaybe(v8::Maybe<T> handle) {
	if (handle.IsNothing()) {
		throw js_error_base();
	} else {
		return handle.FromJust();
	}
}

/**
 * Run a function and annotate the exception with source / line number if it throws
 */
template <typename T>
T RunWithAnnotatedErrors(std::function<T()> fn) {
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

}
