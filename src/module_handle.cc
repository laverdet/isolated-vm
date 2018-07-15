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
		"getModuleRequestsLength", Parameterize<decltype(&ModuleHandle::GetModuleRequestsLength), &ModuleHandle::GetModuleRequestsLength>(),
		"getModuleRequest", Parameterize<decltype(&ModuleHandle::GetModuleRequest<1>), &ModuleHandle::GetModuleRequest<1>>(),
		"getModuleRequestSync", Parameterize<decltype(&ModuleHandle::GetModuleRequest<0>), &ModuleHandle::GetModuleRequest<0>>(),
		"release", Parameterize<decltype(&ModuleHandle::Release), &ModuleHandle::Release>(),
		"link", Parameterize<decltype(&ModuleHandle::Link), &ModuleHandle::Link>(),
		"instantiate", Parameterize<decltype(&ModuleHandle::Instantiate), &ModuleHandle::Instantiate>(),
		"evaluate", Parameterize<decltype(&ModuleHandle::Evaluate), &ModuleHandle::Evaluate>()
	));
}

std::unique_ptr<Transferable> ModuleHandle::TransferOut() {
	return std::make_unique<ModuleHandleTransferable>(isolate, _module);
}



struct GetModuleRequestRunner : public ThreePhaseTask {
	std::unique_ptr<Transferable> result;
	std::shared_ptr<RemoteHandle<v8::Module>> _module;
  std::size_t _index;

	GetModuleRequestRunner(std::shared_ptr<RemoteHandle<v8::Module>> myModule, std::size_t index)
	  : _module(std::move(myModule)), _index(index)
  {
	}

	void Phase2() final {
		auto request = this->_module->Deref()->GetModuleRequest(this->_index);
		result = ExternalCopy::CopyIfPrimitive(request);
	}

	Local<Value> Phase3() final {
		if (result) {
			return result->TransferIn();
		} else {
			return v8::Undefined(v8::Isolate::GetCurrent()).As<v8::Value>();
		}
	}

};



v8::Local<v8::Value> ModuleHandle::GetModuleRequestsLength() {
  const std::size_t length = this->_module->Deref()->GetModuleRequestsLength();
  Isolate* isolate = Isolate::GetCurrent();
  return v8::Integer::New(isolate, length);
}

template <int async>
v8::Local<v8::Value> ModuleHandle::GetModuleRequest(v8::Local<v8::Value> index) {
	return ThreePhaseTask::Run<async, GetModuleRequestRunner>(*this->isolate, this->_module, index->Int32Value());
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

Local<Value> ModuleHandle::Release() {
	_module.reset();
	return Undefined(Isolate::GetCurrent());
}

} // namespace ivm
