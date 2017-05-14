#include <node.h>
#include "util.h"
#include "shareable_isolate.h"
#include "isolate_handle.h"
#include "context_handle.h"
#include "script_handle.h"
#include "reference_handle.h"

#include <memory>

using namespace std;

namespace ivm {

// Module entry point
shared_ptr<ShareableIsolate> root_isolate;
extern "C"
void init(Local<Object> target) {
	Isolate* isolate = Isolate::GetCurrent();
	root_isolate = make_shared<ShareableIsolate>(isolate, isolate->GetCurrentContext());
	target->Set(v8_symbol("Isolate"), ClassHandle::Init<IsolateHandle>());
	target->Set(v8_symbol("Context"), ClassHandle::Init<ContextHandle>());
	target->Set(v8_symbol("Reference"), ClassHandle::Init<ReferenceHandle>());
	target->Set(v8_symbol("Script"), ClassHandle::Init<ScriptHandle>());
}

}

NODE_MODULE(isolated_vm, ivm::init)
