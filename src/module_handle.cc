#include "module_handle.h"
#include "context_handle.h"
#include "reference_handle.h"
#include "transferable.h"
#include "isolate/class_handle.h"
#include "isolate/run_with_timeout.h"
#include "isolate/three_phase_task.h"

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
		dependency_specifiers.emplace_back(*String::Utf8Value{isolate, handle->GetModuleRequest(ii)});
	}
}

ModuleInfo::~ModuleInfo() {
	// Remove from isolate's list of modules
	auto environment = handle.GetIsolateHolder()->GetIsolate();
	if (environment) {
		auto& module_map = environment->module_handles;
		auto range = module_map.equal_range(handle.Deref()->GetIdentityHash());
		auto it = std::find_if(range.first, range.second, [&](decltype(*module_map.begin()) data) {
			return this == data.second;
		});
		assert(it != range.second);
		module_map.erase(it);
	}
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
		"instantiate", Parameterize<decltype(&ModuleHandle::Instantiate), &ModuleHandle::Instantiate>(),
		"instantiateSync", Parameterize<decltype(&ModuleHandle::InstantiateSync), &ModuleHandle::InstantiateSync>(),
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
	size_t length = info->dependency_specifiers.size();
	Local<Array> deps = Array::New(isolate, length);
	for (size_t ii = 0; ii < length; ++ii) {
		Unmaybe(deps->Set(isolate->GetCurrentContext(), ii, v8_string(info->dependency_specifiers[ii].c_str())));
	}
	return deps;
}

std::shared_ptr<ModuleInfo> ModuleHandle::GetInfo() const {
	if (!info) {
		throw js_generic_error("Module has been released");
	}
	return info;
}

/**
 * Implements the module linking logic used by `instantiate`. This is implemented as a class handle
 * so v8 can manage the lifetime of the linker. If a promise fails to resolve then v8 will be
 * responsible for calling the destructor.
 */
class ModuleLinker : public ClassHandle {
	public:
		/**
		 * These methods are split out from the main class so I don't have to recreate the class
		 * inheritance in v8
		 */
		struct Implementation {
			RemoteHandle<Object> linker;
			explicit Implementation(Local<Object> linker) : linker(linker) {}
			virtual ~Implementation() = default;
			virtual void HandleCallbackReturn(ModuleHandle* module, size_t ii, Local<Value> value) = 0;
			virtual Local<Value> Begin(ModuleHandle* module, shared_ptr<RemoteHandle<Context>> context) = 0;
			ModuleLinker& GetLinker() {
				auto ptr = ClassHandle::Unwrap<ModuleLinker>(linker.Deref());
				assert(ptr);
				return *ptr;
			}
		};

	private:
		RemoteHandle<Function> callback;
		std::unique_ptr<Implementation> impl;
		std::vector<std::shared_ptr<ModuleInfo>> modules;

	public:
		static v8::Local<v8::FunctionTemplate> Definition() {
			return MakeClass("Linker", nullptr);
		}

		explicit ModuleLinker(Local<Function> callback) : callback(callback) {}

		~ModuleLinker() override {
			Reset();
		}

		template <typename T>
		void SetImplementation() {
			impl = std::make_unique<T>(This());
		}

		template <typename T>
		T& GetImplementation() {
			return *dynamic_cast<T*>(impl.get());
		}

		Local<Value> Begin(ModuleHandle* module, shared_ptr<RemoteHandle<Context>> context) {
			return impl->Begin(module, std::move(context));
		}

		void ResolveDependency(size_t ii, ModuleInfo& module, ModuleHandle* dependency) {
			{
				// I don't think the lock is actually needed here because this linker has already claimed
				// the whole module, and this code will only be running in a single thread.. but putting
				// up the lock is probably good practice or something.
				std::lock_guard<std::mutex> lock(module.mutex);
				module.resolutions[module.dependency_specifiers[ii]] = dependency->GetInfo();
			}
			Link(dependency);
		}

		void Link(ModuleHandle* module) {
			// Check current link status
			auto info = module->GetInfo();
			{
				std::lock_guard<std::mutex> lock(info->mutex);
				switch (info->link_status) {
					case ModuleInfo::LinkStatus::None:
						info->link_status = ModuleInfo::LinkStatus::Linking;
						info->linker = this;
						break;
					case ModuleInfo::LinkStatus::Linking:
						if (info->linker != this) {
							throw js_generic_error("Module is currently being linked by another linker");
						}
						return;
					case ModuleInfo::LinkStatus::Linked:
						return;
				}
			}
			// Recursively link
			modules.emplace_back(info);
			Isolate* isolate = Isolate::GetCurrent();
			Local<Context> context = isolate->GetCurrentContext();
			Local<Value> recv = Undefined(isolate);
			Local<Value> argv[2];
			argv[1] = module->This();
			Local<Function> fn = callback.Deref();
			for (size_t ii = 0; ii < info->dependency_specifiers.size(); ++ii) {
				argv[0] = v8_string(info->dependency_specifiers[ii].c_str());
				impl->HandleCallbackReturn(module, ii, Unmaybe(fn->Call(context, recv, 2, argv)));
			}
		}

		void Reset(ModuleInfo::LinkStatus status = ModuleInfo::LinkStatus::None) {
			// Clears out dependency info. If the module wasn't instantiated this resets them back to
			// their original state. If it was instantiated then we don't need the dependencies anymore
			// anyway.
			for (auto& module : modules) {
				std::lock_guard<std::mutex> lock(module->mutex);
				module->linker = nullptr;
				module->link_status = status;
				module->resolutions.clear();
			}
			modules.clear();
		}
};

/**
 * Runner for `instantiate`. By the time this is invoked the module will already have all its
 * dependencies resolved by the linker.
 */
struct InstantiateRunner : public ThreePhaseTask {
	shared_ptr<RemoteHandle<Context>> context;
	shared_ptr<ModuleInfo> info;
	RemoteHandle<Object> linker;

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

			if (found != nullptr) {
				// nb: lock is already acquired in `Instantiate`
				auto& resolutions = found->resolutions;
				auto it = resolutions.find(*String::Utf8Value{Isolate::GetCurrent(), specifier});
				if (it != resolutions.end()) {
					ret = it->second->handle.Deref();
				}
			}
			throw js_generic_error("Dependency was left unresolved. Please report this error on github.");
		});
		return ret;
	}

	InstantiateRunner(
		shared_ptr<RemoteHandle<Context>> context,
		shared_ptr<ModuleInfo> info,
		Local<Object> linker
	) :
		context(std::move(context)),
		info(std::move(info)),
		linker(linker) {
		// Sanity check
		if (this->info->handle.GetIsolateHolder() != this->context->GetIsolateHolder()) {
			throw js_generic_error("Invalid context");
		}
	}

	void Phase2() final {
		Local<Module> mod = info->handle.Deref();
		Local<Context> context_local = context->Deref();
		info->context_handle = std::move(context);
		std::lock_guard<std::mutex> lock(info->mutex);
		Unmaybe(mod->InstantiateModule(context_local, ResolveCallback));
	}

	Local<Value> Phase3() final {
		ClassHandle::Unwrap<ModuleLinker>(linker.Deref())->Reset(ModuleInfo::LinkStatus::Linked);
		return Undefined(Isolate::GetCurrent());
	}
};

/**
 * Async / sync implementations of the linker
 */
class ModuleLinkerSync : public ModuleLinker::Implementation {
	private:
		void HandleCallbackReturn(ModuleHandle* module, size_t ii, Local<Value> value) final {
			ModuleHandle* resolved = value->IsObject() ? ClassHandle::Unwrap<ModuleHandle>(value.As<Object>()) : nullptr;
			if (resolved == nullptr) {
				throw js_type_error("Resolved dependency was not `Module`");
			}
			GetLinker().ResolveDependency(ii, *module->GetInfo(), resolved);
		}

	public:
		using ModuleLinker::Implementation::Implementation;
		Local<Value> Begin(ModuleHandle* module, shared_ptr<RemoteHandle<Context>> context) final {
			try {
				GetLinker().Link(module);
			} catch (const js_runtime_error& err) {
				GetLinker().Reset();
				throw;
			}
			auto info = module->GetInfo();
			return ThreePhaseTask::Run<0, InstantiateRunner>(*info->handle.GetIsolateHolder(), context, info, linker.Deref());
		}
};

class ModuleLinkerAsync : public ModuleLinker::Implementation {
	private:
		RemoteTuple<Promise::Resolver, Function> async_handles;
		shared_ptr<RemoteHandle<Context>> context;
		shared_ptr<ModuleInfo> info;
		bool rejected = false;
		uint32_t pending = 0;

		static Local<Value> ModuleResolved(Local<Array> holder, Local<Value> value) {
			FunctorRunners::RunBarrier([&]() {
				ModuleHandle* resolved = value->IsObject() ? ClassHandle::Unwrap<ModuleHandle>(value.As<Object>()) : nullptr;
				if (resolved == nullptr) {
					throw js_type_error("Resolved dependency was not `Module`");
				}
				Local<Context> context = Isolate::GetCurrent()->GetCurrentContext();
				auto linker = ClassHandle::Unwrap<ModuleLinker>(Unmaybe(holder->Get(context, 0)).As<Object>());
				auto& impl = linker->GetImplementation<ModuleLinkerAsync>();
				if (impl.rejected) {
					return;
				}
				auto module = ClassHandle::Unwrap<ModuleHandle>(Unmaybe(holder->Get(context, 1)).As<Object>());
				auto ii = Unmaybe(holder->Get(context, 2)).As<Uint32>()->Value();
				linker->ResolveDependency(ii, *module->GetInfo(), resolved);
				if (--impl.pending == 0) {
					impl.Instantiate();
				}
			});
			return Undefined(Isolate::GetCurrent());
		}

		static Local<Value> ModuleRejected(ModuleLinker* linker, Local<Value> error) {
			FunctorRunners::RunBarrier([&]() {
				auto& impl = linker->GetImplementation<ModuleLinkerAsync>();
				if (!impl.rejected) {
					impl.rejected = true;
					linker->Reset();
					Unmaybe(impl.async_handles.Deref<0>()->Reject(Isolate::GetCurrent()->GetCurrentContext(), error));
				}
			});
			return Undefined(Isolate::GetCurrent());
		}

		void HandleCallbackReturn(ModuleHandle* module, size_t ii, Local<Value> value) final {
			// Resolve via Promise.resolve() so thenables will work
			++pending;
			Isolate* isolate = Isolate::GetCurrent();
			Local<Context> context = isolate->GetCurrentContext();
			Local<Promise::Resolver> resolver = Unmaybe(Promise::Resolver::New(context));
			Local<Promise> promise = resolver->GetPromise();
			Local<Array> holder = Array::New(isolate, 3);
			Unmaybe(holder->Set(context, 0, linker.Deref()));
			Unmaybe(holder->Set(context, 1, module->This()));
			Unmaybe(holder->Set(context, 2, Uint32::New(isolate, ii)));
			promise = Unmaybe(promise->Then(context, ClassHandle::ParameterizeCallback<decltype(&ModuleResolved), &ModuleResolved>(holder)));
			Unmaybe(promise->Catch(context, async_handles.Deref<1>()));
			Unmaybe(resolver->Resolve(context, value));
		}

		void Instantiate() {
			Unmaybe(async_handles.Deref<0>()->Resolve(
				Isolate::GetCurrent()->GetCurrentContext(),
				ThreePhaseTask::Run<1, InstantiateRunner>(*info->handle.GetIsolateHolder(), context, info, linker.Deref())
			));
		}

	public:
		explicit ModuleLinkerAsync(Local<Object> linker) : Implementation(linker), async_handles(
			Unmaybe(Promise::Resolver::New(Isolate::GetCurrent()->GetCurrentContext())),
			ClassHandle::ParameterizeCallback<decltype(&ModuleRejected), &ModuleRejected>(linker)
		) {}

		using ModuleLinker::Implementation::Implementation;
		Local<Value> Begin(ModuleHandle* module, shared_ptr<RemoteHandle<Context>> context) final {
			GetLinker().Link(module);
			info = module->GetInfo();
			this->context = std::move(context);
			if (pending == 0) {
				Instantiate();
			}
			return async_handles.Deref<0>()->GetPromise();
		}
};

Local<Value> ModuleHandle::Instantiate(ContextHandle* context_handle, Local<Function> callback) {
	context_handle->CheckDisposed();
	Local<Object> linker_handle = ClassHandle::NewInstance<ModuleLinker>(callback);
	auto linker = ClassHandle::Unwrap<ModuleLinker>(linker_handle);
	linker->SetImplementation<ModuleLinkerAsync>();
	return linker->Begin(this, context_handle->context);
}

Local<Value> ModuleHandle::InstantiateSync(ContextHandle* context_handle, Local<Function> callback) {
	context_handle->CheckDisposed();
	Local<Object> linker_handle = ClassHandle::NewInstance<ModuleLinker>(callback);
	auto linker = ClassHandle::Unwrap<ModuleLinker>(linker_handle);
	linker->SetImplementation<ModuleLinkerSync>();
	return linker->Begin(this, context_handle->context);
}

struct EvaluateRunner : public ThreePhaseTask {
	shared_ptr<ModuleInfo> info;
	std::unique_ptr<Transferable> result;
	uint32_t timeout;

	EvaluateRunner(shared_ptr<ModuleInfo> info, uint32_t ms) : info(std::move(info)), timeout(ms) {}

	void Phase2() final {
		Local<Module> mod = info->handle.Deref();
		if (mod->GetStatus() == Module::Status::kUninstantiated) {
			throw js_generic_error("Module is uninstantiated");
		}
		Local<Context> context_local = Deref(*info->context_handle);
		Context::Scope context_scope(context_local);
		result = Transferable::OptionalTransferOut(RunWithTimeout(timeout, [&]() { return mod->Evaluate(context_local); }));
		std::lock_guard<std::mutex> lock(info->mutex);
		info->global_namespace = std::make_shared<RemoteHandle<Value>>(mod->GetModuleNamespace());
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
	auto info = GetInfo();
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
	std::lock_guard<std::mutex> lock(info->mutex);
	if (!info->global_namespace) {
		throw js_generic_error("Module has not been instantiated.");
	}
	return ClassHandle::NewInstance<ReferenceHandle>(info->handle.GetSharedIsolateHolder(), info->global_namespace, info->context_handle, ReferenceHandle::TypeOf::Object);
}

} // namespace ivm
