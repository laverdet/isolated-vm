#pragma once
#include "v8_version.h"

namespace ivm {

template <typename Value>
class StringValueWrapper {
	private:
		Value value;
	public:
		StringValueWrapper(v8::Isolate* isolate, v8::Local<v8::Value> string)
#if V8_AT_LEAST(6, 2, 396)
			// `isolate` parameters added in v8 commit fe598532
			: value(isolate, string) {}
#else
			: value(string) {}
#endif
		StringValueWrapper(const StringValueWrapper&) = delete;
		StringValueWrapper& operator=(const StringValueWrapper&) = delete;

		auto operator*() const {
			return *value;
		}

		int length() const {
			return value.length();
		}
};

using Utf8ValueWrapper = StringValueWrapper<v8::String::Utf8Value>;
using Utf16ValueWrapper = StringValueWrapper<v8::String::Utf8Value>;

} // namespace ivm
