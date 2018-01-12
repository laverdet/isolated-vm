#pragma once
#include "shareable_persistent.h"

namespace ivm {

void SharedContextDtor(Persistent<Context>& handle, ShareableIsolate* isolate) {
	if (isolate) {
		auto lhandle = Local<Context>::New(Isolate::GetCurrent(), handle);
		ShareableIsolate::GetCurrent().ContextDestroyed(lhandle);
	}
}

typedef ShareablePersistent<Context, SharedContextDtor> ShareableContext;

};
