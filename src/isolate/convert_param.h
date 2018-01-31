#pragma once
#include <v8.h>
#include "util.h"
#include "class_handle.h"
#include <string>

namespace ivm {

/**
 * Conversion functions for Parameterize
 */
class ClassHandle;
ClassHandle* _ClassHandleUnwrap(v8::Local<v8::Object>);
inline std::string CalleeName(const v8::FunctionCallbackInfo<v8::Value>& info) {
	return std::string("`")+ *v8::String::Utf8Value(info.Data()->ToString())+ "`";
}

inline void RequiresParam(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
	if (ii > info.Length()) {
		throw js_type_error(CalleeName(info)+ " requires at least "+ std::to_string(num)+ " parameter"+ (num == 1 ? "" : "s"));
	}
}

inline void PrivateConstructorError(const v8::FunctionCallbackInfo<v8::Value>& info) {
	throw js_type_error(CalleeName(info)+ " constructor is private");
}

inline void RequireConstructorCall(const v8::FunctionCallbackInfo<v8::Value>& info) {
	if (!info.IsConstructCall()) {
		throw js_type_error(CalleeName(info)+ " must be called with `new`");
	}
}


template <typename T>
struct ConvertParam {
	static T Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info);
};

template <typename T>
struct ConvertParam<v8::Maybe<T*>> {
	static inline v8::Maybe<T*> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		v8::Local<v8::Object> handle;
		if (ii == -1) {
			handle = info.This().As<v8::Object>();
		} else if (ii <= info.Length()) {
			v8::Local<v8::Value> handle_val = info[ii];
			if (!handle_val->IsObject()) {
				return v8::Nothing<T*>();
			}
			handle = handle_val.As<v8::Object>();
			if (handle->InternalFieldCount() != 1) {
				// TODO: Actually check for ClassHandle because otherwise user can pass any native object
				// and crash
				return v8::Nothing<T*>();
			}
		} else {
			return v8::Nothing<T*>();
		}
		T* ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle.As<v8::Object>()));
		if (ptr == nullptr) {
			return v8::Nothing<T*>();
		} else {
			return v8::Just<T*>(ptr);
		}
	}
};

template <typename T>
struct ConvertParam<T*> {
	static inline T* Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		v8::Local<v8::Object> handle;
		if (ii == -1) {
			handle = info.This().As<v8::Object>();
		} else {
			RequiresParam(ii, num, info);
			v8::Local<v8::Value> handle_val = info[ii];
			if (!handle_val->IsObject()) {
				throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an object");
			}
			handle = handle_val.As<v8::Object>();
			if (handle->InternalFieldCount() != 1) {
				// TODO: Actually check for ClassHandle because otherwise user can pass any native object
				// and crash
				throw js_type_error(CalleeName(info)+ " invalid parameter "+ std::to_string(ii + 1));
			}
		}
		T* ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle.As<v8::Object>()));
		if (ptr == nullptr) {
			throw js_type_error(CalleeName(info)+ " invalid parameter "+ std::to_string(ii + 1));
		} else {
			return ptr;
		}
	}
};

template <>
struct ConvertParam<v8::Local<v8::Value>> {
	static inline v8::Local<v8::Value> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		RequiresParam(ii, num, info);
		return info[ii];
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Value>> {
	static inline v8::MaybeLocal<v8::Value> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		if (info.Length() >= ii) {
			return info[ii];
		}
		return v8::MaybeLocal<v8::Value>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::String>> {
	static inline v8::Local<v8::String> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsString()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be a string");
		}
		return info[ii].As<v8::String>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::String>> {
	static inline v8::MaybeLocal<v8::String> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		if (info.Length() >= ii && info[ii]->IsString()) {
			return info[ii].As<v8::String>();
		}
		return v8::MaybeLocal<v8::String>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Number>> {
	static inline v8::Local<v8::Number> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsNumber()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be a number");
		}
		return info[ii].As<v8::Number>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Number>> {
	static inline v8::MaybeLocal<v8::Number> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		if (info.Length() >= ii && info[ii]->IsNumber()) {
			return info[ii].As<v8::Number>();
		}
		return v8::MaybeLocal<v8::Number>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Object>> {
	static inline v8::Local<v8::Object> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsObject()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an object");
		}
		return info[ii].As<v8::Object>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Object>> {
	static inline v8::MaybeLocal<v8::Object> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		if (info.Length() >= ii && info[ii]->IsObject()) {
			return info[ii].As<v8::Object>();
		}
		return v8::MaybeLocal<v8::Object>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Array>> {
	static inline v8::Local<v8::Array> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		RequiresParam(ii, num, info);
		if (!info[ii]->IsArray()) {
			throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be an array");
		}
		return info[ii].As<v8::Array>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Array>> {
	static inline v8::MaybeLocal<v8::Array> Convert(const int ii, const int num, const v8::FunctionCallbackInfo<v8::Value>& info) {
		if (info.Length() >= ii && info[ii]->IsArray()) {
			return info[ii].As<v8::Array>();
		}
		return v8::MaybeLocal<v8::Array>();
	}
};

}
