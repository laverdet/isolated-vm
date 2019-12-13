#pragma once
#include "handle_cast.h"
#include "../util.h"
#include <string>
#include <v8.h>

namespace ivm {
namespace detail {

// Specifies the types which may be default constructed when missing from arguments
template <class Type>
struct IsEmptyHandleAllowed : std::false_type {};

template <class Type>
struct IsEmptyHandleAllowed<Type*> : std::true_type {};

template <class Type>
struct IsEmptyHandleAllowed<v8::Maybe<Type>> : std::true_type {};

template <class Type>
struct IsEmptyHandleAllowed<v8::MaybeLocal<Type>> : std::true_type {};

// These helpers are needed because `if constexpr` isn't supported, otherwise the compiler thinks
// I'm trying to default construct invalid types
struct ParamRequired : std::exception {};

template <class Type>
auto DefaultConstructOrThrow(std::false_type /*false*/) -> Type {
	throw ParamRequired{};
}

template <class Type>
auto DefaultConstructOrThrow(std::true_type /*true*/) -> Type {
	return {};
}

// Returns the name of the current function being called, used for error messages
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

// Extracts parameters from various v8 call signatures
template <int Index>
auto ExtractParamImpl(const v8::FunctionCallbackInfo<v8::Value>& info) -> v8::Local<v8::Value> {
	if (Index == -2) {
		return info.Data();
	} else if (Index == -1) {
		return info.This();
	} else if (Index <= info.Length()) {
		return info[Index];
	} else {
		return {};
	}
}

template <int Index>
auto ExtractParamImpl(v8::Local<v8::String> /*name*/, const v8::PropertyCallbackInfo<v8::Value>& info) -> v8::Local<v8::Value> {
	static_assert(Index == 0, "Getter callback should have no parameters");
	return info.This();
}

template <int Index>
auto ExtractParamImpl(v8::Local<v8::String> /*name*/, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) -> v8::Local<v8::Value> {
	static_assert(Index < 2, "Setter callback should have exactly 1 parameter");
	if (Index == 0) {
		return info.This();
	} else if (Index == 1) {
		return value;
	}
}

// Generic unpacker with empty checking
template <class ...Args>
class ParamExtractor {
	public:
		explicit ParamExtractor(Args... args) : args{args...} {}

		template <int Offset, int Index, class Type>
		decltype(auto) Invoke() {
			constexpr int AdjustedIndex = Index == 0 ? Offset : (std::max(-1, Offset) + Index);
			try {
				v8::Local<v8::Value> value = Extract<AdjustedIndex>(seq);
				if (value.IsEmpty()) {
					return DefaultConstructOrThrow<Type>(detail::IsEmptyHandleAllowed<Type>{});
				} else {
					return Cast<Type>(value, seq);
				}
			} catch (const ParamRequired& ex) {
				throw js_type_error(CalleeName()+ " parameter "+ std::to_string(Index)+ " is required");
			} catch (const ParamIncorrect& ex) {
				if (AdjustedIndex == -1) {
					throw js_type_error(CalleeName()+ " requires `this` to be "+ ex.type);
				} else {
					throw js_type_error(CalleeName()+ " requires parameter "+ std::to_string(AdjustedIndex + 1)+ " to be "+ ex.type);
				}
			}
		}

	private:
		auto CalleeName() {
			return detail::CalleeName(std::get<sizeof...(Args) - 1>(args));
		}

		template <class Type, size_t ...Indices>
		decltype(auto) Cast(v8::Local<v8::Value> value, std::index_sequence<Indices...> /*indices*/) {
			return HandleCast<Type>(value, std::get<Indices>(args)...);
		}

		template <int Index, size_t ...Indices>
		auto Extract(std::index_sequence<Indices...> /*indices*/) {
			return ExtractParamImpl<Index>(std::get<Indices>(args)...);
		}

		std::tuple<Args...> args;
		static constexpr auto seq = std::make_index_sequence<sizeof...(Args)>{};
};

} // namespace detail
} // namespace ivm
