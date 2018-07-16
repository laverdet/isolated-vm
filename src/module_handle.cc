#include "module_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "reference_handle.h"
#include "isolate/class_handle.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

#include <map>
#include <mutex>

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
	: isolate(std::move(isolate)), _module(std::move(myModule)), dependencies(std::make_shared<dependency_map_type>())
{
}

Local<FunctionTemplate> ModuleHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Module", nullptr,
		"getModuleRequestsLength", Parameterize<decltype(&ModuleHandle::GetModuleRequestsLength), &ModuleHandle::GetModuleRequestsLength>(),
		"getModuleRequest", Parameterize<decltype(&ModuleHandle::GetModuleRequest<1>), &ModuleHandle::GetModuleRequest<1>>(),
		"getModuleRequestSync", Parameterize<decltype(&ModuleHandle::GetModuleRequest<0>), &ModuleHandle::GetModuleRequest<0>>(),
		"setDependency", Parameterize<decltype(&ModuleHandle::SetDependency), &ModuleHandle::SetDependency>(),
		"release", Parameterize<decltype(&ModuleHandle::Release), &ModuleHandle::Release>(),
		"instantiate", Parameterize<decltype(&ModuleHandle::Instantiate<1>), &ModuleHandle::Instantiate<1>>(),
		"instantiateSync", Parameterize<decltype(&ModuleHandle::Instantiate<0>), &ModuleHandle::Instantiate<0>>(),
		"evaluate", Parameterize<decltype(&ModuleHandle::Evaluate<1>), &ModuleHandle::Evaluate<1>>(),
		"evaluateSync", Parameterize<decltype(&ModuleHandle::Evaluate<0>), &ModuleHandle::Evaluate<0>>(),
		"getModuleNamespace", Parameterize<decltype(&ModuleHandle::GetModuleNamespace<1>), &ModuleHandle::GetModuleNamespace<1>>(),
		"getModuleNamespaceSync", Parameterize<decltype(&ModuleHandle::GetModuleNamespace<0>), &ModuleHandle::GetModuleNamespace<0>>()
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


v8::Local<v8::Value> ModuleHandle::SetDependency(v8::Local<v8::Value> value, ModuleHandle* module_handle_ptr) {
	if (!value->IsString()) {
		return v8::Boolean::New(v8::Isolate::GetCurrent(), false);
	}
	v8::Local<v8::String> key = value->ToString();
	v8::String::Utf8Value keyAsCharPointer(v8::Isolate::GetCurrent(), key);
	if (!keyAsCharPointer.length()) {
		// UTF8-conversion error probably
		return v8::Boolean::New(v8::Isolate::GetCurrent(), false);
	}
	// Assume the code cannot invoke SetDependency() in parallel, otherwise will the code below not be thread safe
	this->dependencies->insert(dependency_map_type::value_type(*keyAsCharPointer, module_handle_ptr->_module));
	return v8::Boolean::New(v8::Isolate::GetCurrent(), true);
}


std::mutex InstantiateRunnerMutex;
std::shared_ptr<ModuleHandle::dependency_map_type> InstantiateRunnerDependencies;

struct InstantiateRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	std::shared_ptr<RemoteHandle<v8::Module>> _module;
	std::unique_ptr<Transferable> result;
	std::lock_guard<std::mutex> lock_guard;
	
	
	InstantiateRunner(IsolateHolder* isolate, ContextHandle* context_handle, std::shared_ptr<RemoteHandle<v8::Module>> myModule, std::shared_ptr<ModuleHandle::dependency_map_type> deps)
		: context(context_handle->context), _module(std::move(myModule)), lock_guard(InstantiateRunnerMutex)Deref
	{
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
		InstantiateRunnerDependencies = std::move(deps);
	}

	static v8::MaybeLocal<v8::Module> ResolveCallback(v8::Local<v8::Context> context, v8::Local<v8::String> specifierAsLocal, v8::Local<v8::Module> referrer) {
		v8::String::Utf8Value specifierHelper(v8::Isolate::GetCurrent(), specifierAsLocal);
		const std::size_t length = specifierHelper.length();
		if (length) {
			std::string specifierAsString(*specifierHelper, length);
			// We can safely use InstantiateRunnerDependencies
			auto & dependencies = InstantiateRunnerDependencies;
			ModuleHandle::dependency_map_type::iterator it = dependencies->find(specifierAsString);
			if (it != dependencies->end()) {
				return v8::MaybeLocal<v8::Module>(it->second->Deref());
			}
		}
		return v8::MaybeLocal<v8::Module>();
	}

	void Phase2() final {
		v8::Isolate* isolate = v8::Isolate::GetCurrent();
		v8::Maybe<bool> maybe = this->_module->Deref()->InstantiateModule(this->context->Deref(), this->ResolveCallback);
		result = ExternalCopy::CopyIfPrimitive(v8::Boolean::New(isolate, maybe.FromMaybe(false)));
	}

	Local<Value> Phase3() final {
		if (result) {
			return result->TransferIn();
		}
		else {
			return v8::Undefined(v8::Isolate::GetCurrent()).As<v8::Value>();
		}
	}

};

template <int async>
v8::Local<v8::Value> ModuleHandle::Instantiate(ContextHandle* context_handle) {
	if (!this->_module) {
		throw js_generic_error("Module has been released");
	}
	std::shared_ptr<RemoteHandle<v8::Module>> module_ref = this->_module;
	return ThreePhaseTask::Run<async, InstantiateRunner>(*this->isolate, this->isolate.get(), context_handle, std::move(module_ref), this->dependencies);
}



struct EvaluateRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	std::shared_ptr<RemoteHandle<v8::Module>> _module;
	std::unique_ptr<Transferable> result;
	uint32_t timeout;

	EvaluateRunner(IsolateHolder* isolate, ContextHandle* context_handle, std::shared_ptr<RemoteHandle<v8::Module>> myModule, uint32_t t)
		: context(context_handle->context), _module(std::move(myModule)), timeout(t)
	{
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
		Local<Context> context_local = Deref(*this->context);
		Context::Scope context_scope(context_local);
		result = ExternalCopy::CopyIfPrimitive(
			RunWithTimeout(this->timeout, [&]() {
				return this->_module->Deref()->Evaluate(context_local);
			})
		);
	}

	Local<Value> Phase3() final {
		if (result) {
			return result->TransferIn();
		}
		else {
			return v8::Undefined(v8::Isolate::GetCurrent()).As<v8::Value>();
		}
	}

};

template <int async>
v8::Local<v8::Value> ModuleHandle::Evaluate(ContextHandle* context_handle, v8::MaybeLocal<v8::Object> maybe_options) {
	if (!this->_module) {
		throw js_generic_error("Module has been released");
	}
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	uint32_t timeout_ms = 0;
	v8::Local<v8::Object> options;
	if (maybe_options.ToLocal(&options)) {
		v8::Local<v8::Value> timeout_handle = Unmaybe(options->Get(isolate->GetCurrentContext(), v8_string("timeout")));
		if (!timeout_handle->IsUndefined()) {
			if (!timeout_handle->IsUint32()) {
				throw js_type_error("`timeout` must be integer");
			}
			timeout_ms = timeout_handle.As<Uint32>()->Value();
		}
	}
	std::shared_ptr<RemoteHandle<v8::Module>> module_ref = this->_module;
	return ThreePhaseTask::Run<async, EvaluateRunner>(*this->isolate, this->isolate.get(), context_handle, std::move(module_ref), timeout_ms);
}

struct GetModuleNamespaceRunner : public ThreePhaseTask {
	std::shared_ptr<IsolateHolder> isolate;
	std::shared_ptr<RemoteHandle<Context>> context;
	std::shared_ptr<RemoteHandle<v8::Module>> _module;
	//std::unique_ptr<ReferenceHandle> result;
	std::shared_ptr<RemoteHandle<v8::Value>> result;
	
	GetModuleNamespaceRunner(std::shared_ptr<IsolateHolder> myIsolate, ContextHandle* context_handle, std::shared_ptr<RemoteHandle<v8::Module>> myModule)
		: isolate(std::move(myIsolate)), context(context_handle->context), _module(std::move(myModule))
	{
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate.get() != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
		v8::Local<v8::Context> context_handle = ivm::Deref(*this->context);
		v8::Context::Scope context_scope(context_handle);
		v8::Local<v8::Value> moduleNamespace = this->_module->Deref()->GetModuleNamespace();
		result = std::make_shared<RemoteHandle<v8::Value>>(moduleNamespace);
	}


	Local<Value> Phase3() final {
		if (this->result) {
			Local<Object> value = ClassHandle::NewInstance<ReferenceHandle>(this->isolate, this->result, this->context, ReferenceHandle::TypeOf::Object);
			return value;
		}
		return v8::Undefined(v8::Isolate::GetCurrent());
	}

};

template <int async>
v8::Local<v8::Value> ModuleHandle::GetModuleNamespace(ContextHandle* context_handle) {
	std::shared_ptr<RemoteHandle<v8::Module>> module_ref = this->_module;
	return ThreePhaseTask::Run<async, GetModuleNamespaceRunner>(*this->isolate, this->isolate, context_handle, std::move(module_ref));
}


Local<Value> ModuleHandle::Release() {
	_module.reset();
	return Undefined(Isolate::GetCurrent());
}



} // namespace ivm
