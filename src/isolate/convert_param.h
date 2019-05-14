#pragma once
#include <v8.h>
#include "util.h"
#include "class_handle.h"
#include <string>

namespace ivm {

/**
 * Conversion functions for Parameterize
 */
class ClassHandle* _ClassHandleUnwrap(v8::Local<v8::Object> handle);

inline std::string CalleeName(const v8::FunctionCallbackInfo<v8::Value>& info) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	return std::string("`")+ *v8::String::Utf8Value{isolate, Unmaybe(info.Data()->ToString(isolate->GetCurrentContext()))}+ "`";
}

inline std::string CalleeName(const v8::PropertyCallbackInfo<v8::Value>& info) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	return std::string("`")+ *v8::String::Utf8Value{isolate, Unmaybe(info.Data()->ToString(isolate->GetCurrentContext()))}+ "`";
}

inline std::string CalleeName(const v8::PropertyCallbackInfo<void>& info) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	return std::string("`")+ *v8::String::Utf8Value{isolate, Unmaybe(info.Data()->ToString(isolate->GetCurrentContext()))}+ "`";
}

inline void PrivateConstructorError(const v8::FunctionCallbackInfo<v8::Value>& info) {
	throw js_type_error(CalleeName(info)+ " constructor is private");
}

inline void RequireConstructorCall(const v8::FunctionCallbackInfo<v8::Value>& info) {
	if (!info.IsConstructCall()) {
		throw js_type_error(CalleeName(info)+ " must be called with `new`");
	}
}

/**
 * Exceptions used by ConvertParam to signal invalid parameters.
 */
struct param_required : public std::exception {};

struct param_incorrect : public std::exception {
	const char* type;
	explicit param_incorrect(const char* type) : type(type) {}
};

/**
 * Invoker for ConvertParam
 */
template <typename T, int ii, int num>
struct ConvertParamInvoke {
	// Function
	static auto Invoke(const v8::FunctionCallbackInfo<v8::Value>& info) {
		try {
			if (ii == -1) {
				return T::Convert(info.This());
			} else if (ii == -2) {
				return T::Convert(info.Data());
			} else if (ii <= info.Length()) {
				return T::Convert(info[ii]);
			} else {
				return T::Convert(v8::Local<v8::Value>());
			}
		} catch (const param_required& ex) {
			throw js_type_error(CalleeName(info)+ " requires at least "+ std::to_string(num)+ (num == 1 ? " parameter" : "parameters"));
		} catch (const param_incorrect& ex) {
			if (ii == -1) {
				throw js_type_error(CalleeName(info)+ " requires `this` to be "+ ex.type);
			} else {
				throw js_type_error(CalleeName(info)+ " requires parameter "+ std::to_string(ii + 1)+ " to be "+ ex.type);
			}
		}
	}
	// Getter
	static auto Invoke(const v8::PropertyCallbackInfo<v8::Value>& info) {
		try {
			if (ii == -1) {
				return T::Convert(info.This());
			} else {
				assert(false);
			}
		} catch (const param_incorrect& ex) {
			throw js_type_error(CalleeName(info)+ " getter requires `this` to be "+ ex.type);
		}
	}
	// Setter
	static auto Invoke(std::pair<v8::Local<v8::Value>*, const v8::PropertyCallbackInfo<void>*> info) {
		try {
			if (ii == -1) {
				return T::Convert(info.second->This());
			} else if (ii == 0) {
				return T::Convert(*info.first);
			} else {
				assert(false);
			}
		} catch (const param_incorrect& ex) {
			if (ii == -1) {
				throw js_type_error(CalleeName(*info.second)+ " setter requires `this` to be "+ ex.type);
			} else {
				throw js_type_error(CalleeName(*info.second)+ " must be "+ ex.type);
			}
		}
	}

};

/**
 * v8 param conversions
 */
template <typename T>
struct ConvertParam {
	static T Convert(v8::Local<v8::Value> param);
};

template <typename T>
struct ConvertParam<v8::Maybe<T*>> {
	static v8::Maybe<T*> Convert(const v8::Local<v8::Value> param) {
		if (param.IsEmpty() || !param->IsObject()) {
			return v8::Nothing<T*>();
		}
		v8::Local<v8::Object> handle = param.As<v8::Object>();
		if (handle->InternalFieldCount() != 1) {
			// TODO: Actually check for ClassHandle because otherwise user can pass any native object
			// and crash
			return v8::Nothing<T*>();
		}
		auto ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle));
		if (ptr == nullptr) {
			return v8::Nothing<T*>();
		} else {
			return v8::Just<T*>(ptr);
		}
	}
};

template <typename T>
struct ConvertParam<T*> {
	static T* Convert(const v8::Local<v8::Value> param) {
		if (param.IsEmpty()) {
			throw param_required();
		} else if (!param->IsObject()) {
			throw param_incorrect("an object");
		}
		v8::Local<v8::Object> handle = param.As<v8::Object>();
		if (handle->InternalFieldCount() != 1) {
			// TODO: Actually check for ClassHandle because otherwise user can pass any native object
			// and crash
			throw param_incorrect("something else");
		}
		auto ptr = dynamic_cast<T*>(_ClassHandleUnwrap(handle));
		if (ptr == nullptr) {
			throw param_incorrect("something else");
		} else {
			return ptr;
		}
	}
};

template <>
struct ConvertParam<v8::Local<v8::Value>> {
	static v8::Local<v8::Value> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			throw param_required();
		}
		return param;
	}
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Value>> {
	static v8::MaybeLocal<v8::Value> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			return {};
		}
		return param;
	}
};

template <>
struct ConvertParam<v8::Local<v8::String>> {
	static v8::Local<v8::String> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			throw param_required();
		} else if (!param->IsString()) {
			throw param_incorrect("a string");
		}
		return param.As<v8::String>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::String>> {
	static v8::MaybeLocal<v8::String> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty() || param->IsUndefined()) {
			return {};
		} else if (!param->IsString()) {
			throw param_incorrect("a string");
		}
		return param.As<v8::String>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Object>> {
	static v8::Local<v8::Object> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			throw param_required();
		} else if (!param->IsObject()) {
			throw param_incorrect("an object");
		}
		return param.As<v8::Object>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Object>> {
	static v8::MaybeLocal<v8::Object> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty() || param->IsUndefined()) {
			return {};
		} else if (!param->IsObject()) {
			throw param_incorrect("an object");
		}
		return param.As<v8::Object>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Array>> {
	static v8::Local<v8::Array> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			throw param_required();
		} else if (!param->IsArray()) {
			throw param_incorrect("an array");
		}
		return param.As<v8::Array>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Array>> {
	static v8::MaybeLocal<v8::Array> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty() || param->IsUndefined()) {
			return {};
		} else if (!param->IsArray()) {
			throw param_incorrect("an array");
		}
		return param.As<v8::Array>();
	}
};

template <>
struct ConvertParam<v8::Local<v8::Function>> {
	static v8::Local<v8::Function> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty()) {
			throw param_required();
		} else if (!param->IsFunction()) {
			throw param_incorrect("a function");
		}
		return param.As<v8::Function>();
	};
};

template <>
struct ConvertParam<v8::MaybeLocal<v8::Function>> {
	static v8::MaybeLocal<v8::Function> Convert(const v8::Local<v8::Value>& param) {
		if (param.IsEmpty() || param->IsUndefined()) {
			return {};
		} else if (!param->IsFunction()) {
			throw param_incorrect("a function");
		}
		return param.As<v8::Function>();
	}
};

} // namespace ivm
