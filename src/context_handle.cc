#include "context_handle.h"
#include "reference_handle.h"

using namespace v8;
using std::shared_ptr;

namespace ivm {

ContextHandle::ContextHandleTransferable::ContextHandleTransferable(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Context>> context,
	shared_ptr<Persistent<Value>> global
) : isolate(std::move(isolate)), context(std::move(context)), global(std::move(global)) {}

Local<Value> ContextHandle::ContextHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ContextHandle>(isolate, context, global);
}

ContextHandle::ContextHandle(
	shared_ptr<IsolateHolder> isolate,
	shared_ptr<Persistent<Context>> context,
	shared_ptr<Persistent<Value>> global
) : isolate(std::move(isolate)), context(std::move(context)), global(std::move(global)) {}

IsolateEnvironment::IsolateSpecific<FunctionTemplate>& ContextHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> ContextHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Context", nullptr,
		"globalReference", Parameterize<decltype(&ContextHandle::GlobalReference), &ContextHandle::GlobalReference>(),
		"release", Parameterize<decltype(&ContextHandle::Release), &ContextHandle::Release>()
	));
}

std::unique_ptr<Transferable> ContextHandle::TransferOut() {
	return std::make_unique<ContextHandleTransferable>(isolate, context, global);
}

void ContextHandle::CheckDisposed() {
	if (!context) {
		throw js_generic_error("Context is released");
	}
}

Local<Value> ContextHandle::GlobalReference() {
	CheckDisposed();
	return ClassHandle::NewInstance<ReferenceHandle>(isolate, global, context, ReferenceHandle::TypeOf::Object);
}

Local<Value> ContextHandle::Release() {
	CheckDisposed();
	isolate.reset();
	context.reset();
	global.reset();
	return Undefined(Isolate::GetCurrent());
}

} // namespace ivm
