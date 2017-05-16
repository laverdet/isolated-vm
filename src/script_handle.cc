#include "script_handle.h"
#include "external_copy.h"
#include "transferable.h"

namespace ivm {
using namespace std;
using namespace v8;

Local<Value> ScriptHandle::RunSync(ContextHandle* context_handle) {
	return ShareableIsolate::Locker(script->GetIsolate(), [ this, context_handle ]() {
		Local<Context> context = context_handle->context->Deref();
		Context::Scope context_scope(context);
		Local<Script> script = this->script->Deref()->BindToCurrentContext();
		Local<Value> result = Unmaybe(script->Run(context));
		std::unique_ptr<Transferable> result_copy = ExternalCopy::CopyIfPrimitive(result);
		if (result_copy.get() != nullptr) {
			return result_copy;
		} else {
			return Transferable::TransferOut(Undefined(Isolate::GetCurrent()));
		}
	})->TransferIn();
}

}
