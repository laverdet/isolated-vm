#include "module_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

using namespace v8;
using std::shared_ptr;

namespace ivm {

ModuleHandle::ModuleHandleTransferable::ModuleHandleTransferable(shared_ptr<IsolateHolder> isolate, shared_ptr<RemoteHandle<Module>> myModule)
	: isolate(std::move(isolate)), _module(std::move(myModule))
{
}

Local<Value> ModuleHandle::ModuleHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ModuleHandle>(isolate, _module);
};

ModuleHandle::ModuleHandle(shared_ptr<IsolateHolder> isolate, shared_ptr<RemoteHandle<Module>> myModule)
	: isolate(std::move(isolate)), _module(std::move(myModule))
{
}

Local<FunctionTemplate> ModuleHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Module", nullptr,
		"release", Parameterize<decltype(&ModuleHandle::Release), &ModuleHandle::Release>(),
		"link", Parameterize<decltype(&ModuleHandle::Link), &ModuleHandle::Link>(),
		"instantiate", Parameterize<decltype(&ModuleHandle::Instantiate), &ModuleHandle::Instantiate>(),
		"evaluate", Parameterize<decltype(&ModuleHandle::Evaluate), &ModuleHandle::Evaluate>()
	));
}

std::unique_ptr<Transferable> ModuleHandle::TransferOut() {
	return std::make_unique<ModuleHandleTransferable>(isolate, _module);
}

Local<Value> ModuleHandle::Release() {
	_module.reset();
	return Undefined(Isolate::GetCurrent());
}


//template <int async>
Local<Value> ModuleHandle::Link(Local<Function> linker) {
	throw js_generic_error("Not yet implemented");
}


Local<Value> ModuleHandle::Instantiate() {
	throw js_generic_error("Not yet implemented");
}

//template <int async>
Local<Value> ModuleHandle::Evaluate(v8::MaybeLocal<v8::Object> options) {
	throw js_generic_error("Not yet implemented");
}

} // namespace ivm
