#pragma once
#include <v8.h>
#include <memory>
#include <mutex>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"

namespace ivm {

struct ModuleInfo {
	// Underlying data on the module. Some information is stored outside of v8 so there is a separate
	// struct to hold this data, which is then referenced by any number of handles.
	friend struct InstantiateRunner;
	friend class ModuleHandle;
	std::mutex mutex;
	std::vector<std::string> dependency_specifiers;
	std::unordered_map<std::string, std::shared_ptr<ModuleInfo>> resolutions;
	RemoteHandle<v8::Module> handle;
	std::shared_ptr<RemoteHandle<v8::Context>> context_handle;
	std::shared_ptr<RemoteHandle<v8::Value>> global_namespace;
	explicit ModuleInfo(v8::Local<v8::Module> handle);
	~ModuleInfo();
};

class ModuleHandle : public TransferableHandle {
	private:
		class ModuleHandleTransferable : public Transferable {
			private:
				std::shared_ptr<ModuleInfo> info;
			public:
				explicit ModuleHandleTransferable(std::shared_ptr<ModuleInfo> info);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<ModuleInfo> info;

	public:
		explicit ModuleHandle(std::shared_ptr<ModuleInfo> info);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		v8::Local<v8::Value> GetDependencySpecifiers();

		v8::Local<v8::Value> SetDependency(v8::Local<v8::String> specifier_handle, ModuleHandle* module_handle);

		template <int async>
		v8::Local<v8::Value> Instantiate(class ContextHandle* context_handle);

		template <int async>
		v8::Local<v8::Value> Evaluate(v8::MaybeLocal<v8::Object> maybe_options);

		v8::Local<v8::Value> GetNamespace();
};

} // namespace ivm
