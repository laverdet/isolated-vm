#include "module_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "reference_handle.h"
#include "isolate/class_handle.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"
#include "isolate/legacy.h"

#include <algorithm>
#include <mutex>


using namespace v8;
using std::shared_ptr;

namespace ivm {

IsolatedModule::IsolatedModule(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<RemoteHandle<v8::Module>> handle, std::vector<std::string> dependencySpecifiers)
	: isolate(std::move(isolate)), module_handle(std::move(handle)), dependencySpecifiers(std::move(dependencySpecifiers))
{
}


void IsolatedModule::lock()
{
	return mutex.lock();
}

void IsolatedModule::unlock()
{
	return mutex.unlock();
}


const std::vector<std::string> & IsolatedModule::GetDependencySpecifiers() const
{
	return dependencySpecifiers;
}

void IsolatedModule::SetDependency(std::string specifier, std::shared_ptr<IsolatedModule> isolated_module) {
	// Probably not a good idea because if dynamic import() anytime get supported()
	//
	// if (std::find(dependencySpecifiers.begin(), dependencySpecifiers.end(), specifier) == dependencySpecifiers.end()) {
	//	const std::string errorMessage = "Module has no dependency named: \"" + specifier + "\".";
	//	throw js_generic_error(errorMessage.c_str());
	// }
	resolutions[specifier] = isolated_module;
}



MaybeLocal<Module> IsolatedModule::ResolveCallback(Local<Context> context, Local<String> specifier, Local<Module> referrer) {
	//	We want to use the current global data

	std::string dependency = *Utf8ValueWrapper(Isolate::GetCurrent(), specifier);
	
	//return Undefined(Isolate::GetCurrent());
	/*
	if (length) {
		std::string specifierAsString(*specifierHelper, length);
		// We can safely use InstantiateRunnerDependencies
		auto & dependencies = InstantiateRunnerDependencies;
		ModuleHandle::dependency_map_type::iterator it = dependencies->find(specifierAsString);
		if (it != dependencies->end()) {
			return MaybeLocal<Module>(it->second->Deref());
		}
	}*/
	
	throw js_generic_error((std::string("Failed to resolve dependency: ") + dependency).c_str());
	return MaybeLocal<Module>();
}


void IsolatedModule::Instantiate(std::shared_ptr<RemoteHandle<v8::Context>> c) {
	std::lock_guard<IsolatedModule> lock(*this);
	context_handle = c;
	Isolate* isolate = Isolate::GetCurrent();
	Local<Context> context = context_handle->Deref();
	Local<Module> mod = module_handle->Deref();
	bool result = Unmaybe(mod->InstantiateModule(context, IsolatedModule::ResolveCallback));
	if (!result) {
		throw js_generic_error("Failed to instantiate module");
	}
}

std::unique_ptr<Transferable> IsolatedModule::Evaluate(std::size_t timeout) {
	std::lock_guard<IsolatedModule> lock(*this);
	Local<Context> context_local = Deref(*context_handle);
	Context::Scope context_scope(context_local);
	Local<Module> mod = module_handle->Deref();
	return ExternalCopy::CopyIfPrimitive(RunWithTimeout(timeout, [&]() {
		auto returnValue = mod->Evaluate(context_local);
		Local<Value> moduleNamespace = mod->GetModuleNamespace();
		global_namespace = std::make_shared<RemoteHandle<Value>>(moduleNamespace);
		return returnValue;
	}));
}


Local<Value> IsolatedModule::GetNamespace() {
	Local<Object> value = ClassHandle::NewInstance<ReferenceHandle>(isolate, global_namespace, context_handle, ReferenceHandle::TypeOf::Object);
	return value;
}


ModuleHandle::ModuleHandleTransferable::ModuleHandleTransferable(shared_ptr<IsolateHolder> isolate, shared_ptr<IsolatedModule> isolated_module)
	: isolate(std::move(isolate)), isolated_module(std::move(isolated_module))
{
}

Local<Value> ModuleHandle::ModuleHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ModuleHandle>(isolate, isolated_module);
};

ModuleHandle::ModuleHandle(shared_ptr<IsolateHolder> isolate, shared_ptr<IsolatedModule> isolated_module)
	: isolate(std::move(isolate)), isolated_module(std::move(isolated_module))
{
}

Local<FunctionTemplate> ModuleHandle::Definition() {
	return Inherit<TransferableHandle>(MakeClass(
		"Module", nullptr,
		"dependencySpecifiers", ParameterizeAccessor<decltype(&ModuleHandle::GetDependencySpecifiers), &ModuleHandle::GetDependencySpecifiers>(),
		"setDependency", Parameterize<decltype(&ModuleHandle::SetDependency), &ModuleHandle::SetDependency>(),
		"instantiate", Parameterize<decltype(&ModuleHandle::Instantiate<1>), &ModuleHandle::Instantiate<1>>(),
		"instantiateSync", Parameterize<decltype(&ModuleHandle::Instantiate<0>), &ModuleHandle::Instantiate<0>>(),
		"evaluate", Parameterize<decltype(&ModuleHandle::Evaluate<1>), &ModuleHandle::Evaluate<1>>(),
		"evaluateSync", Parameterize<decltype(&ModuleHandle::Evaluate<0>), &ModuleHandle::Evaluate<0>>(),
		"namespace", ParameterizeAccessor<decltype(&ModuleHandle::GetNamespace), &ModuleHandle::GetNamespace>()
		//"getModuleNamespace", Parameterize<decltype(&ModuleHandle::GetModuleNamespace<1>), &ModuleHandle::GetModuleNamespace<1>>(),
		//"getModuleNamespaceSync", Parameterize<decltype(&ModuleHandle::GetModuleNamespace<0>), &ModuleHandle::GetModuleNamespace<0>>()
		/*"release", Parameterize<decltype(&ModuleHandle::Release), &ModuleHandle::Release>(),
		*/
	));
}

std::unique_ptr<Transferable> ModuleHandle::TransferOut() {
	return std::make_unique<ModuleHandleTransferable>(isolate, isolated_module);
}

Local<Value> ModuleHandle::GetDependencySpecifiers() {
	
	std::lock_guard<IsolatedModule> lock(*isolated_module);
	const std::vector<std::string> & dependencySpecifiers = isolated_module->GetDependencySpecifiers();
	std::size_t size = dependencySpecifiers.size();

	Local<Array> deps = Array::New(Isolate::GetCurrent(), size);

	if (deps.IsEmpty()) {
		return Local<Array>();
	}
	
	for (std::size_t index = 0; index < size; ++index) {
		deps->Set(index, v8_string(dependencySpecifiers[index].c_str()));
	}
	return deps;
}


Local<Value> ModuleHandle::SetDependency(Local<String> value, ModuleHandle* module_handle) {
	std::string dependency = *Utf8ValueWrapper(Isolate::GetCurrent(), value);
	isolated_module->SetDependency(dependency, module_handle->isolated_module);
}


struct InstantiateRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<IsolatedModule> isolated_module;
	
	InstantiateRunner(IsolateHolder* isolate, ContextHandle* context_handle, shared_ptr<IsolatedModule> isolated_module)
		: context(context_handle->context), isolated_module(std::move(isolated_module))
	{
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context 1233");
		}
	}

	void Phase2() final {
		isolated_module->Instantiate(context);
	}

	Local<Value> Phase3() final {
		return Undefined(Isolate::GetCurrent());
	}

};

template <int async>
Local<Value> ModuleHandle::Instantiate(ContextHandle* context_handle) {
	if (!isolated_module) {
		throw js_generic_error("Module has been released");
	}
	return ThreePhaseTask::Run<async, InstantiateRunner>(*isolate, isolate.get(), context_handle, isolated_module);
}


struct EvaluateRunner : public ThreePhaseTask {
	shared_ptr<IsolatedModule> isolated_module;
	std::unique_ptr<Transferable> result;
	uint32_t timeout;

	EvaluateRunner(shared_ptr<IsolatedModule> isolated_module, uint32_t t)
		: isolated_module(std::move(isolated_module)), timeout(t)
	{
	}

	void Phase2() final {
		result = isolated_module->Evaluate(timeout);
	}

	Local<Value> Phase3() final {
		return result->TransferIn();
	}

};

template <int async>
Local<Value> ModuleHandle::Evaluate(MaybeLocal<Object> maybe_options) {
	if (!isolated_module) {
		throw js_generic_error("Module has been released");
	}
	Isolate* isolate = Isolate::GetCurrent();
	uint32_t timeout_ms = 0;
	Local<Object> options;
	if (maybe_options.ToLocal(&options)) {
		Local<Value> timeout_handle = Unmaybe(options->Get(isolate->GetCurrentContext(), v8_string("timeout")));
		if (!timeout_handle->IsUndefined()) {
			if (!timeout_handle->IsUint32()) {
				throw js_type_error("`timeout` must be integer");
			}
			timeout_ms = timeout_handle.As<Uint32>()->Value();
		}
	}
	return ThreePhaseTask::Run<async, EvaluateRunner>(*this->isolate, isolated_module, timeout_ms);
}


Local<Value> ModuleHandle::GetNamespace() {
	return isolated_module->GetNamespace();
}

/*

struct GetModuleNamespaceRunner : public ThreePhaseTask {
	std::shared_ptr<IsolateHolder> isolate;
	std::shared_ptr<RemoteHandle<Context>> context;
	std::shared_ptr<RemoteHandle<Module>> _module;
	//std::unique_ptr<ReferenceHandle> result;
	std::shared_ptr<RemoteHandle<Value>> result;
	
	GetModuleNamespaceRunner(std::shared_ptr<IsolateHolder> myIsolate, ContextHandle* context_handle, std::shared_ptr<RemoteHandle<Module>> myModule)
		: isolate(std::move(myIsolate)), context(context_handle->context), _module(std::move(myModule))
	{
		// Sanity check
		context_handle->CheckDisposed();
		if (isolate.get() != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
		Local<Context> context_handle = ivm::Deref(*this->context);
		Context::Scope context_scope(context_handle);
		Local<Value> moduleNamespace = this->_module->Deref()->GetModuleNamespace();
		result = std::make_shared<RemoteHandle<Value>>(moduleNamespace);
	}


	Local<Value> Phase3() final {
		if (this->result) {
			Local<Object> value = ClassHandle::NewInstance<ReferenceHandle>(this->isolate, this->result, this->context, ReferenceHandle::TypeOf::Object);
			return value;
		}
		return Undefined(Isolate::GetCurrent());
	}

};

template <int async>
Local<Value> ModuleHandle::GetModuleNamespace(ContextHandle* context_handle) {
	std::shared_ptr<RemoteHandle<Module>> module_ref = this->_module;
	return ThreePhaseTask::Run<async, GetModuleNamespaceRunner>(*this->isolate, this->isolate, context_handle, std::move(module_ref));
}


Local<Value> ModuleHandle::Release() {
	_module.reset();
	return Undefined(Isolate::GetCurrent());
}
*/


} // namespace ivm
