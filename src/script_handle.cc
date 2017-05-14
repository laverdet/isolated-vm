#include "script_handle.h"
#include "transferable.h"

namespace ivm {
using namespace std;
using namespace v8;

void ScriptHandle::RunSync(const FunctionCallbackInfo<Value>& args) {
	if (args.Length() < 1) {
		THROW(Exception::TypeError, "runSync expects 1 parameter");
	}
	ContextHandle* context = ClassHandle::Unwrap<ContextHandle>(Local<Object>::Cast(args[0]));
	auto result = ShareableIsolate::Locker(script->GetIsolate(), [ this, context ]() -> unique_ptr<Transferable> {
		TryCatch try_catch(Isolate::GetCurrent());
		Context::Scope context_scope(context->context->Deref());
		Local<Script> script = this->script->Deref()->BindToCurrentContext();
		Local<Value> result = script->Run();
		if (try_catch.HasCaught()) {
			try_catch.ReThrow();
			return nullptr;
		}
		return Transferable::TransferOut(result);
	});
	if (result.get() != nullptr) {
		args.GetReturnValue().Set(result->TransferIn());
	}
}

}
