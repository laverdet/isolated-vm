#pragma once
#include <node.h>
#include "../util.h"
#include "class_handle.h"
#include <string>

namespace ivm {
using namespace v8;

/**
 * Conversion functions for Parameterize
 */
class ClassHandle;
ClassHandle* _ClassHandleUnwrap(Local<Object>);
inline std::string CalleeName(const FunctionCallbackInfo<Value>& info) {
	return std::string("`")+ *String::Utf8Value(info.Data()->ToString())+ "`";
}

inline void RequiresParam(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
	if (ii > info.Length()) {
		throw js_type_error(CalleeName(info)+ " requires at least "+ std::to_string(num)+ " parameter"+ (num == 1 ? "" : "s"));
	}
}

inline void PrivateConstructorError(const FunctionCallbackInfo<Value>& info) {
	throw js_type_error(CalleeName(info)+ " constructor is private");
}

inline void RequireConstructorCall(const FunctionCallbackInfo<Value>& info) {
	if (!info.IsConstructCall()) {
		throw js_type_error(CalleeName(info)+ " must be called with `new`");
	}
}


template <typename T>
struct ConvertParam {
	static T Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info);
};

template <typename T>
struct ConvertParam<Maybe<T*>> {
	static inline Maybe<T*> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		Local<Object> handle;
		if (ii == -1) {
			handle = info.This().As<Object>();
		} else if (ii <= info.Length()) {
			Local<Value> handle_val = info[ii];
			if (!handle_val->IsObject()) {
				return Nothing<T*>();
			}
			handle = handle_val.As<Object>();
			if (handle->InternalFieldCount() != 1) {
				// TODO: Actually check for ClassHandle because otherwise user can pass any native object
				// and crash
				return Nothing<T*>();
			}
		} else {
			return Nothing<T*>();
		}
		T* ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle.As<Object>()));
		if (ptr == nullptr) {
			return Nothing<T*>();
		} else {
			return Just<T*>(ptr);
		}
	}
};

template <typename T>
struct ConvertParam<T*> {
	static inline T* Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		Local<Object> handle;
		if (ii == -1) {
			handle = info.This().As<Object>();
		} else {
			RequiresParam(ii, num, info);
			Local<Value> handle_val = info[ii];
			if (!handle_val->IsObject()) {
				throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an object");
			}
			handle = handle_val.As<Object>();
			if (handle->InternalFieldCount() != 1) {
				// TODO: Actually check for ClassHandle because otherwise user can pass any native object
				// and crash
				throw js_type_error(CalleeName(info)+ " invalid parameter "+ std::to_string(ii + 1));
			}
		}
		T* ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle.As<Object>()));
		if (ptr == nullptr) {
			throw js_type_error(CalleeName(info)+ " invalid parameter "+ std::to_string(ii + 1));
		} else {
			return ptr;
		}
	}
};

template <>
struct ConvertParam<Local<Value>> {
	static inline Local<Value> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		RequiresParam(ii, num, info);
		return info[ii];
	};
};

template <>
struct ConvertParam<MaybeLocal<Value>> {
	static inline MaybeLocal<Value> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		if (info.Length() >= ii) {
			return info[ii];
		}
		return MaybeLocal<Value>();
	}
};

template <>
struct ConvertParam<Local<String>> {
	static inline Local<String> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsString()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be a string");
		}
		return info[ii].As<String>();
	};
};

template <>
struct ConvertParam<MaybeLocal<String>> {
	static inline MaybeLocal<String> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		if (info.Length() >= ii && info[ii]->IsString()) {
			return info[ii].As<String>();
		}
		return MaybeLocal<String>();
	}
};

template <>
struct ConvertParam<Local<Number>> {
	static inline Local<Number> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsNumber()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be a number");
		}
		return info[ii].As<Number>();
	};
};

template <>
struct ConvertParam<MaybeLocal<Number>> {
	static inline MaybeLocal<Number> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		if (info.Length() >= ii && info[ii]->IsNumber()) {
			return info[ii].As<Number>();
		}
		return MaybeLocal<Number>();
	}
};

template <>
struct ConvertParam<Local<Object>> {
	static inline Local<Object> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsObject()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an object");
		}
		return info[ii].As<Object>();
	};
};

template <>
struct ConvertParam<MaybeLocal<Object>> {
	static inline MaybeLocal<Object> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		if (info.Length() >= ii && info[ii]->IsObject()) {
			return info[ii].As<Object>();
		}
		return MaybeLocal<Object>();
	}
};

template <>
struct ConvertParam<Local<Array>> {
	static inline Local<Array> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsArray()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an array");
		}
		return info[ii].As<Array>();
	};
};

template <>
struct ConvertParam<MaybeLocal<Array>> {
	static inline MaybeLocal<Array> Convert(const int ii, const int num, const FunctionCallbackInfo<Value>& info) {
		if (info.Length() >= ii && info[ii]->IsArray()) {
			return info[ii].As<Array>();
		}
		return MaybeLocal<Array>();
	}
};

}
