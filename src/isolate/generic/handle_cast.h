#pragma once
#include <v8.h>
#include "../util.h" // TODO: remove

namespace ivm {

// Internal handle conversion error
struct ParamIncorrect : std::exception {
	explicit ParamIncorrect(const char* type) : type{type} {}
	const char* type;
};

// Common arguments for casting functions
struct HandleCastArguments {
	HandleCastArguments(bool strict, v8::Isolate* isolate, v8::Local<v8::Context> context) :
		isolate{isolate}, context{context}, strict{strict} {}

	v8::Isolate* isolate;
	v8::Local<v8::Context> context;
	bool strict;
};

// Helpers
template <class Type>
struct HandleCastTag {};

template <class Value>
struct HandleCaster {
	explicit HandleCaster(Value value, HandleCastArguments arguments) : value{value}, arguments{arguments} {}

	template <class Type>
	operator Type() { // NOLINT(hicpp-explicit-conversions)
		return HandleCastImpl(value, arguments, HandleCastTag<Type>{});
	}

	Value value;
	HandleCastArguments arguments;
};

// Explicit casts: printf("%d\n", HandleCast<int>(value));
template <class Type, class Value>
decltype(auto) HandleCast(Value value, HandleCastArguments arguments, HandleCastTag<Type> /*tag*/ = {}) {
	return HandleCaster<Value>{value, arguments}.operator Type();
}

template <class Type, class Value>
decltype(auto) HandleCast(
	Value value, bool strict = true,
	v8::Isolate* isolate = v8::Isolate::GetCurrent(),
	v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext(),
	HandleCastTag<Type> /*tag*/ = {}
) {
	return HandleCast<Type>(value, HandleCastArguments{strict, isolate, context});
}

// Implicit cast: int32_t number = HandleCast(value);
template <class Value>
decltype(auto) HandleCast(Value value, HandleCastArguments arguments) {
	return HandleCaster<Value>{std::forward<Value>(value), arguments};
}

template <class Value>
decltype(auto) HandleCast(
	Value value, bool strict = true,
	v8::Isolate* isolate = v8::Isolate::GetCurrent(),
	v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext()
) {
	return HandleCast(std::forward<Value>(value), HandleCastArguments{strict, isolate, context});
}

// Provides arguments from FunctionCallback
template <class Type>
decltype(auto) HandleCast(v8::Local<v8::Value> value, const v8::FunctionCallbackInfo<v8::Value>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<v8::Local<v8::Value>>{value, HandleCastArguments{true, isolate, isolate->GetCurrentContext()}}.operator Type();
}

// Provides arguments from GetterCallback
template <class Type>
decltype(auto) HandleCast(v8::Local<v8::Value> value, v8::Local<v8::String> /*name*/, const v8::PropertyCallbackInfo<v8::Value>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<v8::Local<v8::Value>>{value, HandleCastArguments{true, isolate, isolate->GetCurrentContext()}}.operator Type();
}

// Provides arguments from SetterCallback
template <class Type>
decltype(auto) HandleCast(v8::Local<v8::Value> value, v8::Local<v8::String> /*name*/, v8::Local<v8::Value> /*value*/, const v8::PropertyCallbackInfo<void>& info) {
	auto isolate = info.GetIsolate();
	return HandleCaster<v8::Local<v8::Value>>{value, HandleCastArguments{true, isolate, isolate->GetCurrentContext()}}.operator Type();
}

// Identity cast
inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::Local<v8::Value>> /*tag*/) {
	return value;
}

// Local<Value> -> Local<...> conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::Local<v8::Array>> /*tag*/) {
	if (value->IsArray()) {
		return value.As<v8::Array>();
	}
	throw ParamIncorrect{"an array"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments arguments, HandleCastTag<v8::Local<v8::Boolean>> /*tag*/) {
	if (value->IsBoolean()) {
		return value.As<v8::Boolean>();
	} else if (!arguments.strict) {
		return value->ToBoolean(arguments.isolate);
	}
	throw ParamIncorrect{"a boolean"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::Local<v8::Function>> /*tag*/) {
	if (value->IsFunction()) {
		return value.As<v8::Function>();
	}
	throw ParamIncorrect{"a function"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::Local<v8::Object>> /*tag*/) {
	if (value->IsObject()) {
		return value.As<v8::Object>();
	}
	throw ParamIncorrect{"an object"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments arguments, HandleCastTag<v8::Local<v8::String>> /*tag*/) {
	if (value->IsString()) {
		return value.As<v8::String>();
	} else if (!arguments.strict) {
		return Unmaybe(value->ToString(arguments.context));
	}
	throw ParamIncorrect{"a string"};
}

// Local<Value> -> MaybeLocal<...> conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Array>> /*tag*/)
-> v8::MaybeLocal<v8::Array> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsArray()) {
		return {value.As<v8::Array>()};
	}
	throw ParamIncorrect{"an array"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Function>> /*tag*/)
-> v8::MaybeLocal<v8::Function> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsFunction()) {
		return {value.As<v8::Function>()};
	}
	throw ParamIncorrect{"a function"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Object>> /*tag*/)
-> v8::MaybeLocal<v8::Object> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsObject()) {
		return {value.As<v8::Object>()};
	}
	throw ParamIncorrect{"an object"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments arguments, HandleCastTag<v8::MaybeLocal<v8::String>> /*tag*/)
-> v8::MaybeLocal<v8::String> {
	if (value->IsNullOrUndefined()) {
		return {};
	} else if (value->IsString()) {
		return {value.As<v8::String>()};
	} else if (!arguments.strict) {
		return {Unmaybe(value->ToString(arguments.context))};
	}
	throw ParamIncorrect{"a string"};
}

inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments /*arguments*/, HandleCastTag<v8::MaybeLocal<v8::Value>> /*tag*/)
-> v8::MaybeLocal<v8::Value> {
	return value;
}

// Local<...> -> native C++ conversions
inline auto HandleCastImpl(v8::Local<v8::Value> value, HandleCastArguments arguments, HandleCastTag<bool> /*tag*/) {
	return HandleCast(HandleCast<v8::Local<v8::Boolean>>(value, arguments), arguments);
}

inline auto HandleCastImpl(v8::Local<v8::Boolean> value, HandleCastArguments /*arguments*/, HandleCastTag<bool> /*tag*/) {
	return value->Value();
}

} // namespace ivm
