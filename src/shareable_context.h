#pragma once
#include "shareable_persistent.h"

namespace ivm {

void SharedContextDtor(Persistent<Context>& handle, ShareableIsolate* isolate);
//typedef ShareablePersistent<Context, SharedContextDtor> ShareableContext;
typedef ShareablePersistent<Context> ShareableContext;

};
