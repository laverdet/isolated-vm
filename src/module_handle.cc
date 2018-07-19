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

std::mutex IsolatedModule::shared::mutex;
std::unordered_multimap<int, IsolatedModule*> IsolatedModule::shared::available_modules;

IsolatedModule* IsolatedModule::shared::find(v8::Local<v8::Module> handle) {
	std::lock_guard<std::mutex> lock(mutex);
	int hash = handle->GetIdentityHash();
	auto range = available_modules.equal_range(hash);
	
	auto it = std::find_if(range.first, range.second, [&](const available_modules_type::value_type & data) {
		return data.second->module_handle->Deref() == handle;
	});
	return it == range.second ? nullptr : it->second;
}

void IsolatedModule::shared::add(IsolatedModule* ptr) {
	std::lock_guard<std::mutex> lock(mutex);
	available_modules.emplace(ptr->module_handle->Deref()->GetIdentityHash(), ptr);
}

void IsolatedModule::shared::remove(IsolatedModule* ptr) {
	std::lock_guard<std::mutex> lock(mutex);
	int hash = ptr->module_handle->Deref()->GetIdentityHash();
	auto range = available_modules.equal_range(hash);
	auto it = std::find_if(range.first, range.second, [&](const available_modules_type::value_type & data) {
		return ptr == data.second;
	});
	if (it != range.second) {
		available_modules.erase(it);
	}
}


IsolatedModule::IsolatedModule(std::shared_ptr<IsolateHolder> isolate, std::shared_ptr<RemoteHandle<v8::Module>> handle, std::vector<std::string> dependency_specifiers)
    : isolate(std::move(isolate)),
      dependency_specifiers(std::move(dependency_specifiers)),
      module_handle(std::move(handle))
{
  shared::add(this);
}

IsolatedModule::~IsolatedModule() {
	shared::remove(this);
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
	return dependency_specifiers;
}

void IsolatedModule::SetDependency(std::string specifier, std::shared_ptr<IsolatedModule> isolated_module) {
	// Probably not a good idea because if dynamic import() anytime get supported()
	std::lock_guard<IsolatedModule> lock(*this);
	if (std::find(GetDependencySpecifiers().begin(), GetDependencySpecifiers().end(), specifier) == GetDependencySpecifiers().end()) {
	  const std::string errorMessage = "Module has no dependency named: \"" + specifier + "\".";
	  throw js_generic_error(errorMessage.c_str());
	}
	resolutions[specifier] = std::move(isolated_module);
}



MaybeLocal<Module> IsolatedModule::ResolveCallback(Local<Context> context, Local<String> specifier, Local<Module> referrer) {
	
	std::string dependency = *Utf8ValueWrapper(Isolate::GetCurrent(), specifier);
	IsolatedModule* found = shared::find(referrer);
	
	std::string base_error_message = std::string("Failed to resolve dependency: \"") + dependency + std::string("\". ");

	if (found != nullptr) {
		// I don´t believe we must check the context, but I am unsure
		if (found->context_handle->Deref() != context) {
			throw js_generic_error(base_error_message + std::string("Invalid context"));
		}

		std::lock_guard<IsolatedModule> lock(*found);
		auto & resolutions = found->resolutions;
		auto it = resolutions.find(dependency);
		

		if (it != resolutions.end()) {
			return it->second->module_handle->Deref();
		}
	}
	throw js_generic_error(base_error_message);
}


void IsolatedModule::Instantiate(std::shared_ptr<RemoteHandle<v8::Context>> _context_handle) {
	std::lock_guard<IsolatedModule> lock(*this);
	context_handle = std::move(_context_handle);
	Local<Context> context = context_handle->Deref();
	Local<Module> mod = module_handle->Deref();
	Unmaybe(mod->InstantiateModule(context, IsolatedModule::ResolveCallback)); // Assume the Unmaybe will throw an exception if InstantiateModule returns false
}

std::unique_ptr<Transferable> IsolatedModule::Evaluate(std::size_t timeout) {
	std::lock_guard<IsolatedModule> lock(*this);
	Local<Context> context_local = Deref(*context_handle);
	Context::Scope context_scope(context_local);
	Local<Module> mod = module_handle->Deref();
	std::unique_ptr<Transferable> ret = ExternalCopy::CopyIfPrimitive(RunWithTimeout(timeout, [&]() { return mod->Evaluate(context_local); }));
	global_namespace =std::make_shared<RemoteHandle<Value>>(mod->GetModuleNamespace());
	return ret;
}


Local<Value> IsolatedModule::GetNamespace() {
	if (!global_namespace) {
		throw js_generic_error("No namespace object exist. Have you evaluated the module?");
	}
	Local<Object> value = ClassHandle::NewInstance<ReferenceHandle>(isolate, global_namespace, context_handle, ReferenceHandle::TypeOf::Object);
	return value;
}



bool operator==(const IsolatedModule& isolated_module, const v8::Local<v8::Module>& mod) {
	return isolated_module.module_handle->Deref() == mod;
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
	
	for (std::size_t index = 0; index < size; ++index) {
		deps->Set(index, v8_string(dependencySpecifiers[index].c_str()));
	}
	return deps;
}


Local<Value> ModuleHandle::SetDependency(Local<String> value, ModuleHandle* module_handle) {
	std::string dependency = *Utf8ValueWrapper(Isolate::GetCurrent(), value);
	isolated_module->SetDependency(dependency, module_handle->isolated_module);
	return Undefined(Isolate::GetCurrent());
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
			throw js_generic_error("Invalid context");
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

} // namespace ivm
