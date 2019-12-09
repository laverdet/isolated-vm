#include "context_handle.h"
#include "reference_handle.h"

using namespace v8;

namespace ivm {

ContextHandle::ContextHandleTransferable::ContextHandleTransferable(
	RemoteHandle<Context> context,
	RemoteHandle<Value> global
) : context{std::move(context)}, global{std::move(global)} {}

Local<Value> ContextHandle::ContextHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ContextHandle>(context, global);
}

ContextHandle::ContextHandle(
	RemoteHandle<Context> context,
	RemoteHandle<Value> global
) : context(std::move(context)), global(std::move(global)) {}

Local<FunctionTemplate> ContextHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Context", nullptr,
		"global", ParameterizeAccessor<
			decltype(&ContextHandle::GlobalGetter), &ContextHandle::GlobalGetter,
			decltype(&ContextHandle::GlobalSetter), &ContextHandle::GlobalSetter
		>(),
		"release", Parameterize<decltype(&ContextHandle::Release), &ContextHandle::Release>()
	));
}

std::unique_ptr<Transferable> ContextHandle::TransferOut() {
	return std::make_unique<ContextHandleTransferable>(context, global);
}

void ContextHandle::CheckDisposed() {
	if (!context) {
		throw js_generic_error("Context is released");
	}
}

Local<Value> ContextHandle::GlobalGetter() {
	Isolate* isolate = Isolate::GetCurrent();
	if (!context) {
		return Undefined(isolate);
	}
	Local<Object> ref;
	if (global_reference) {
		ref = Deref(global_reference);
	} else {
		ref = ClassHandle::NewInstance<ReferenceHandle>(global.GetSharedIsolateHolder(), global, context, ReferenceHandle::TypeOf::Object);
		global_reference = RemoteHandle<v8::Object>(ref);
	}
	Unmaybe(This()->CreateDataProperty(isolate->GetCurrentContext(), v8_string("global"), ref));
	return ref;
}

void ContextHandle::GlobalSetter(Local<Value> value) {
	Unmaybe(This()->CreateDataProperty(Isolate::GetCurrent()->GetCurrentContext(), v8_string("global"), value));
}

Local<Value> ContextHandle::Release() {
	CheckDisposed();
	context = {};
	global = {};
	if (global_reference) {
		ClassHandle::Unwrap<ReferenceHandle>(Deref(global_reference))->Release();
		global_reference = {};
	}
	return Undefined(Isolate::GetCurrent());
}

} // namespace ivm
