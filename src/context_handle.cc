#include "context_handle.h"
#include "reference_handle.h"
#include "transferable.h"

using namespace v8;

namespace ivm {
namespace {

/**
 * Instances of this turn into a ContextHandle when they are transferred in
 */
class ContextHandleTransferable : public Transferable {
	public:
		ContextHandleTransferable(RemoteHandle<Context> context, RemoteHandle<Value> global) :
			context{std::move(context)}, global{std::move(global)} {}

		auto TransferIn() -> Local<Value> final {
			return ClassHandle::NewInstance<ContextHandle>(std::move(context), std::move(global));
		}

	private:
		RemoteHandle<v8::Context> context;
		RemoteHandle<v8::Value> global;
};

} // anonymous namespace

/**
 * ContextHandle implementation
 */
ContextHandle::ContextHandle(RemoteHandle<Context> context, RemoteHandle<Value> global) :
	context{std::move(context)}, global{std::move(global)} {}

Local<FunctionTemplate> ContextHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Context", nullptr,
		"global", ParameterizeAccessor<decltype(&ContextHandle::GlobalGetter), &ContextHandle::GlobalGetter>(),
		"release", Parameterize<decltype(&ContextHandle::Release), &ContextHandle::Release>()
	));
}

auto ContextHandle::TransferOut() -> std::unique_ptr<Transferable> {
	return std::make_unique<ContextHandleTransferable>(context, global);
}

auto ContextHandle::GetContext() const -> RemoteHandle<v8::Context> {
	if (!context) {
		throw js_generic_error("Context is released");
	}
	return context;
}

auto ContextHandle::GlobalGetter() -> Local<Value> {
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

auto ContextHandle::Release() -> Local<Value> {
	return Boolean::New(Isolate::GetCurrent(), [&]() {
		if (context) {
			context = {};
			global = {};
			if (global_reference) {
				ClassHandle::Unwrap<ReferenceHandle>(Deref(global_reference))->Release();
				global_reference = {};
			}
			return true;
		} else {
			return false;
		}
	}());
}

} // namespace ivm
