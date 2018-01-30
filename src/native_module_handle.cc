#include "native_module_handle.h"
#include "context_handle.h"
#include "reference_handle.h"
#include "isolate/environment.h"
#include "isolate/three_phase_task.h"

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * RAII wrapper around libuv dlopen
 */
NativeModuleHandle::NativeModule::NativeModule(const std::string& filename) : init(nullptr) {
	if (uv_dlopen(filename.c_str(), &lib)) {
		throw js_generic_error("Failed to load module");
	}
	if (uv_dlsym(&lib, "InitForContext", reinterpret_cast<void**>(&init)) || init == nullptr) {
		uv_dlclose(&lib);
		throw js_generic_error("Module is not isolated-vm compatible");
	}
}

NativeModuleHandle::NativeModule::~NativeModule() {
	uv_dlclose(&lib);
}

void NativeModuleHandle::NativeModule::InitForContext(Isolate* isolate, Local<Context> context, Local<Object> target) {
	init(isolate, context, target);
}

/**
 * Simple transferable logic so we can transfer native module handle between isolates
 */
NativeModuleHandle::NativeModuleTransferable::NativeModuleTransferable(shared_ptr<NativeModule> module) : module(std::move(module)) {}

Local<Value> NativeModuleHandle::NativeModuleTransferable::TransferIn() {
	return ClassHandle::NewInstance<NativeModuleHandle>(std::move(module));
}

/**
 * Native module JS API
 */
NativeModuleHandle::NativeModuleHandle(shared_ptr<NativeModule> module) : module(std::move(module)) {}

IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& NativeModuleHandle::TemplateSpecific() {
	static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
	return tmpl;
}

Local<FunctionTemplate> NativeModuleHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"NativeModule", ParameterizeCtor<decltype(&New), &New>, 1,
		"create", Parameterize<decltype(&NativeModuleHandle::Create<true>), &NativeModuleHandle::Create<true>>, 1,
		"createSync", Parameterize<decltype(&NativeModuleHandle::Create<false>), &NativeModuleHandle::Create<false>>, 1
	));
}

unique_ptr<NativeModuleHandle> NativeModuleHandle::New(Local<String> value) {
	return std::make_unique<NativeModuleHandle>(
		std::make_shared<NativeModule>(*String::Utf8Value(value))
	);
}

unique_ptr<Transferable> NativeModuleHandle::TransferOut() {
	return std::make_unique<NativeModuleTransferable>(module);
}

template <bool async>
Local<Value> NativeModuleHandle::Create(class ContextHandle* context_handle) {
	class Create : public ThreePhaseTask {
		private:
			shared_ptr<Persistent<Context>> context;
			shared_ptr<NativeModuleHandle::NativeModule> module;
			unique_ptr<Transferable> result;

		public:
			Create(shared_ptr<Persistent<Context>> context, shared_ptr<NativeModuleHandle::NativeModule> module) : context(context), module(module) {}

		protected:
			void Phase2() final {
				Isolate* isolate = Isolate::GetCurrent();
				Local<Context> context_handle = Deref(*context);
				Context::Scope context_scope(context_handle);
				Local<Object> exports = Object::New(isolate);
				module->InitForContext(isolate, context_handle, exports);
				result = ReferenceHandle::New(exports)->TransferOut();
			}

			Local<Value> Phase3() final {
				return result->TransferIn();
			}
	};
	return ThreePhaseTask::Run<async, Create>(*context_handle->isolate, context_handle->context, module);
}

}
