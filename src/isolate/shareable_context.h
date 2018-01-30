#pragma once
#include "shareable_persistent.h"

namespace ivm {

void SharedContextDtor(Persistent<Context>& handle, IsolateEnvironment* isolate);
//typedef ShareablePersistent<Context, SharedContextDtor> ShareableContext;
typedef ShareablePersistent<Context> ShareableContext;

};
