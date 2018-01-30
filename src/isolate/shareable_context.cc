#include "shareable_context.h"

namespace ivm {

void SharedContextDtor(Persistent<Context>& handle, IsolateEnvironment* isolate) {
	if (isolate) {
		auto lhandle = Local<Context>::New(Isolate::GetCurrent(), handle);
//		IsolateEnvironment::GetCurrent().ContextDestroyed(lhandle);
	}
}

};
