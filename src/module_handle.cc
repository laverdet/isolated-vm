#include "module_handle.h"
#include "context_handle.h"
#include "external_copy.h"
#include "reference_handle.h"
#include "isolate/class_handle.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"
#include "isolate/legacy.h"

#include <algorithm>

using namespace v8;
using std::shared_ptr;

namespace ivm {

ModuleInfo::ModuleInfo(Local<Module> handle) : handle(handle) {
	// Add to isolate's list of modules
	IsolateEnvironment::GetCurrent()->module_handles.emplace(handle->GetIdentityHash(), this);
	// Grab all dependency specifiers
	Isolate* isolate = Isolate::GetCurrent();
	size_t length = handle->GetModuleRequestsLength();
	dependency_specifiers.reserve(length);
	for (size_t ii = 0; ii < length; ++ii) {
		dependency_specifiers.emplace_back(*Utf8ValueWrapper(isolate, handle->GetModuleRequest(ii)));
	}
}

ModuleInfo::~ModuleInfo() {
	// Remove from isolate's list of modules
	auto& module_map = IsolateEnvironment::GetCurrent()->module_handles;
	auto range = module_map.equal_range(handle.Deref()->GetIdentityHash());
	auto it = std::find_if(range.first, range.second, [&](decltype(*module_map.begin()) data) {
		return this == data.second;
	});
	assert(it != range.second);
	module_map.erase(it);
}

ModuleHandle::ModuleHandleTransferable::ModuleHandleTransferable(shared_ptr<ModuleInfo> info) : info(std::move(info)) {}

Local<Value> ModuleHandle::ModuleHandleTransferable::TransferIn() {
	return ClassHandle::NewInstance<ModuleHandle>(info);
};

ModuleHandle::ModuleHandle(shared_ptr<ModuleInfo> info) : info(std::move(info)) {}

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
	));
}

std::unique_ptr<Transferable> ModuleHandle::TransferOut() {
	return std::make_unique<ModuleHandleTransferable>(info);
}

Local<Value> ModuleHandle::GetDependencySpecifiers() {
	Isolate* isolate = Isolate::GetCurrent();
	std::lock_guard<std::mutex> lock(info->mutex);
	size_t length = info->dependency_specifiers.size();
	Local<Array> deps = Array::New(isolate, length);
	for (size_t ii = 0; ii < length; ++ii) {
		Unmaybe(deps->Set(isolate->GetCurrentContext(), ii, v8_string(info->dependency_specifiers[ii].c_str())));
	}
	return deps;
}


Local<Value> ModuleHandle::SetDependency(Local<String> specifier_handle, ModuleHandle* module_handle) {
	// Probably not a good idea because if dynamic import() anytime get supported()
	std::string specifier = *Utf8ValueWrapper(Isolate::GetCurrent(), specifier_handle);
	std::lock_guard<std::mutex> lock(info->mutex);
	if (std::find(info->dependency_specifiers.begin(), info->dependency_specifiers.end(), specifier) == info->dependency_specifiers.end()) {
		throw js_generic_error(std::string("Module has no dependency named: \"") + specifier + "\".");
	}
	info->resolutions[specifier] = module_handle->info;
	return Undefined(Isolate::GetCurrent());
}


struct InstantiateRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<ModuleInfo> info;

	static MaybeLocal<Module> ResolveCallback(Local<Context> /* context */, Local<String> specifier, Local<Module> referrer) {
		MaybeLocal<Module> ret;
		FunctorRunners::RunBarrier([&]() {
			// Lookup ModuleInfo* instance from `referrer`
			auto& module_map = IsolateEnvironment::GetCurrent()->module_handles;
			auto range = module_map.equal_range(referrer->GetIdentityHash());
			auto it = std::find_if(range.first, range.second, [&](decltype(*module_map.begin()) data) {
				return data.second->handle.Deref() == referrer;
			});
			ModuleInfo* found = it == range.second ? nullptr : it->second;

			std::string dependency = *Utf8ValueWrapper(Isolate::GetCurrent(), specifier);

			if (found != nullptr) {
				// nb: lock is already acquired in `Instantiate`
				auto& resolutions = found->resolutions;
				auto it = resolutions.find(*Utf8ValueWrapper(Isolate::GetCurrent(), specifier));
				if (it != resolutions.end()) {
					ret = it->second->handle.Deref();
				}
			}
			throw js_generic_error(std::string("Failed to resolve dependency: \"") + dependency + "\".");
		});
		return ret;
	}

	InstantiateRunner(
		ContextHandle* context_handle,
		shared_ptr<ModuleInfo> info
	) :
		context(context_handle->context),
		info(std::move(info)) {
		// Sanity check
		context_handle->CheckDisposed();
		if (this->info->handle.GetIsolateHolder() != context_handle->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
#if V8_AT_LEAST(6, 1, 328)
		Local<Module> mod = info->handle.Deref();
		if (mod->GetStatus() != Module::Status::kUninstantiated) {
			throw js_generic_error("Module is already instantiated");
		}
		Local<Context> context_local = context->Deref();
		info->context_handle = std::move(context);
		std::lock_guard<std::mutex> lock(info->mutex);
		Unmaybe(mod->InstantiateModule(context_local, ResolveCallback));
#endif
	}

	Local<Value> Phase3() final {
		return Undefined(Isolate::GetCurrent());
	}

};

template <int async>
Local<Value> ModuleHandle::Instantiate(ContextHandle* context_handle) {
	if (!info) {
		throw js_generic_error("Module has been released");
	}
	return ThreePhaseTask::Run<async, InstantiateRunner>(*info->handle.GetIsolateHolder(), context_handle, info);
}


struct EvaluateRunner : public ThreePhaseTask {
	shared_ptr<ModuleInfo> info;
	std::unique_ptr<Transferable> result;
	uint32_t timeout;

	EvaluateRunner(shared_ptr<ModuleInfo> info, uint32_t ms) : info(std::move(info)), timeout(ms) {}

	void Phase2() final {
#if V8_AT_LEAST(6, 1, 328)
		Local<Module> mod = info->handle.Deref();
		if (mod->GetStatus() != Module::Status::kInstantiated) {
			throw js_generic_error("Module is uninitialized or already evaluated");
		}
		Local<Context> context_local = Deref(*info->context_handle);
		Context::Scope context_scope(context_local);
		result = ExternalCopy::CopyIfPrimitive(RunWithTimeout(timeout, [&]() { return mod->Evaluate(context_local); }));
		std::lock_guard<std::mutex> lock(info->mutex);
		info->global_namespace = std::make_shared<RemoteHandle<Value>>(mod->GetModuleNamespace());
#endif
	}

	Local<Value> Phase3() final {
		if (result) {
			return result->TransferIn();
		} else {
			return Undefined(Isolate::GetCurrent()).As<Value>();
		}
	}
};

template <int async>
Local<Value> ModuleHandle::Evaluate(MaybeLocal<Object> maybe_options) {
	if (!info) {
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
	return ThreePhaseTask::Run<async, EvaluateRunner>(*info->handle.GetIsolateHolder(), info, timeout_ms);
}

Local<Value> ModuleHandle::GetNamespace() {
#if V8_AT_LEAST(6, 1, 328)
	std::lock_guard<std::mutex> lock(info->mutex);
	if (!info->global_namespace) {
		throw js_generic_error("Module has not been instantiated.");
	}
	return ClassHandle::NewInstance<ReferenceHandle>(info->handle.GetSharedIsolateHolder(), info->global_namespace, info->context_handle, ReferenceHandle::TypeOf::Object);
#else
	throw js_generic_error("Module has not been instantiated.");
#endif
}

} // namespace ivm
