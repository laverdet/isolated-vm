#include "shareable_context.h"

namespace ivm {

void SharedContextDtor(v8::Persistent<v8::Context>& handle, IsolateEnvironment* isolate) {
	if (isolate) {
		auto lhandle = v8::Local<v8::Context>::New(v8::Isolate::GetCurrent(), handle);
//		IsolateEnvironment::GetCurrent().ContextDestroyed(lhandle);
	}
}

};
