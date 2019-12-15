#pragma once
#include <v8.h>
#include "./error.h"

namespace ivm {

// Internal handle conversion error. All these `HandleCast` functions are templated and inlined and
// throwing generates verbose asm so this is implementated as a static function to clean up the
// typical case
struct ParamIncorrect : std::exception {
	explicit ParamIncorrect(const char* type) : type{type} {}
	[[noreturn]] static void Throw(const char* type) { throw ParamIncorrect{type}; }
	const char* type;
};

// Common arguments for casting functions
class HandleCastArguments {
	private:
		// Most of the time we won't need the context (and it is never needed in "strict" casting mode).
		// This little holder will only call GetCurrentContext() when needed because the optimizer can't
		// elide the call in any circumstances
		class ContextHolder {
			public:
				explicit ContextHolder(v8::Isolate* isolate) : isolate{isolate} {}
				inline operator v8::Local<v8::Context>() const { // NOLINT(hicpp-explicit-conversions)
					if (context.IsEmpty()) {
						context = isolate->GetCurrentContext();
					}
					return context;
				}

			private:
				v8::Isolate* const isolate;
				mutable v8::Local<v8::Context> context{};
		};

	public:
		HandleCastArguments(bool strict, v8::Isolate* isolate) :
			isolate{isolate}, context{isolate}, strict{strict} {}

		v8::Isolate* const isolate;
		const ContextHolder context;
		const bool strict;
};

// Helpers
template <class Type>
struct HandleCastTag {};

template <class Value>
struct HandleCaster {
	explicit HandleCaster(Value value, HandleCastArguments arguments) : value{value}, arguments{std::move(arguments)} {}

	template <class Type>
	operator Type() { // NOLINT(hicpp-explicit-conversions)
		return HandleCastImpl(value, arguments, HandleCastTag<Type>{});
	}

	Value value;
	HandleCastArguments arguments;
};

// Explicit casts: printf("%d\n", HandleCast<int>(value));
template <class Type, class Value>
decltype(auto) HandleCast(Value value, HandleCastArguments arguments) {
	return HandleCastImpl(value, arguments, HandleCastTag<Type>{});
}

template <class Type, class Value>
decltype(auto) HandleCast(
	Value value, bool strict = true,
	v8::Isolate* isolate = v8::Isolate::GetCurrent()
) {
	return HandleCast<Type>(value, HandleCastArguments{strict, isolate});
}

// Implicit cast: int32_t number = HandleCast(value);
template <class Value>
inline decltype(auto) HandleCast(Value value, HandleCastArguments arguments) {
	return HandleCaster<Value>{std::forward<Value>(value), arguments};
}

template <class Value>
inline decltype(auto) HandleCast(
	Value value, bool strict = true,
	v8::Isolate* isolate = v8::Isolate::GetCurrent()
) {
	return HandleCast(std::forward<Value>(value), HandleCastArguments{strict, isolate});
}

// Provides arguments from FunctionCallback
template <class Type, class Value>
inline decltype(auto) HandleCast(Value value, const v8::FunctionCallbackInfo<v8::Value>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<Value>{value, HandleCastArguments{true, isolate}}.operator Type();
}

// Provides arguments from GetterCallback
template <class Type>
inline decltype(auto) HandleCast(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<v8::Local<v8::Value>>{value, HandleCastArguments{true, isolate}}.operator Type();
}

// Provides arguments from SetterCallback
template <class Type>
inline decltype(auto) HandleCast(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<v8::Local<v8::Value>>{value, HandleCastArguments{true, isolate}}.operator Type();
}

// Identity cast
inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::Local<v8::Value>> /*tag*/) {
	return value;
}

// Local<Value> -> Local<...> conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::Local<v8::Array>> /*tag*/) {
	if (value->IsArray()) {
		return value.As<v8::Array>();
	}
	ParamIncorrect::Throw("an array");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& arguments, HandleCastTag<v8::Local<v8::Boolean>> /*tag*/) {
	if (value->IsBoolean()) {
		return value.As<v8::Boolean>();
	} else if (!arguments.strict) {
		return value->ToBoolean(arguments.isolate);
	}
	ParamIncorrect::Throw("a boolean");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::Local<v8::Function>> /*tag*/) {
	if (value->IsFunction()) {
		return value.As<v8::Function>();
	}
	ParamIncorrect::Throw("a function");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::Local<v8::Object>> /*tag*/) {
	if (value->IsObject()) {
		return value.As<v8::Object>();
	}
	ParamIncorrect::Throw("an object");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& arguments, HandleCastTag<v8::Local<v8::String>> /*tag*/) {
	if (value->IsString()) {
		return value.As<v8::String>();
	} else if (!arguments.strict) {
		return Unmaybe(value->ToString(arguments.context));
	}
	ParamIncorrect::Throw("a string");
}

// Local<Value> -> MaybeLocal<...> conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Array>> /*tag*/)
-> v8::MaybeLocal<v8::Array> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsArray()) {
		return {value.As<v8::Array>()};
	}
	ParamIncorrect::Throw("an array");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Function>> /*tag*/)
-> v8::MaybeLocal<v8::Function> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsFunction()) {
		return {value.As<v8::Function>()};
	}
	ParamIncorrect::Throw("a function");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Object>> /*tag*/)
-> v8::MaybeLocal<v8::Object> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsObject()) {
		return {value.As<v8::Object>()};
	}
	ParamIncorrect::Throw("an object");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& arguments, HandleCastTag<v8::MaybeLocal<v8::String>> /*tag*/)
-> v8::MaybeLocal<v8::String> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsString()) {
		return {value.As<v8::String>()};
	} else if (!arguments.strict) {
		return {Unmaybe(value->ToString(arguments.context))};
	}
	ParamIncorrect::Throw("a string");
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Value>> /*tag*/) {
	return v8::MaybeLocal<v8::Value>{value};
}

// Local<...> -> native C++ conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, const HandleCastArguments& arguments, HandleCastTag<bool> /*tag*/) {
	return HandleCast(HandleCast<v8::Local<v8::Boolean>>(value, arguments), arguments);
}

inline auto HandleCastImpl(v8::Local<v8::Boolean> value, const HandleCastArguments& /*arguments*/, HandleCastTag<bool> /*tag*/) {
	return value->Value();
}

// native C++ -> Local<Value> conversions
inline auto HandleCastImpl(bool value, const HandleCastArguments& arguments, HandleCastTag<v8::Local<v8::Value>> /*tag*/) {
	return v8::Boolean::New(arguments.isolate, value).As<v8::Value>();
}

} // namespace ivm
