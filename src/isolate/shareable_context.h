#pragma once
#include "shareable_persistent.h"

namespace ivm {

void SharedContextDtor(v8::Persistent<v8::Context>& handle, IsolateEnvironment* isolate);
//typedef ShareablePersistent<Context, SharedContextDtor> ShareableContext;
typedef ShareablePersistent<v8::Context> ShareableContext;

};
