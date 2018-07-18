#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>
#include <unordered_set>

namespace ivm {


class IsolatedModule : public std::enable_shared_from_this<IsolatedModule>  { // we need this to access the parent shared_ptr
private:
	struct shared {
		// My idea is we could some how store information here so all modules could be resolved using only this information.
		// in that case may it be simple to support dynamic  import() too
		static std::mutex mutex;
		static std::unordered_set<IsolatedModule> available_modules;
	};

	std::mutex mutex;
	std::shared_ptr<IsolateHolder> isolate; // required by ClassHandle used to construct the namespace reference
	std::vector<std::string> dependencySpecifiers;
	std::shared_ptr<RemoteHandle<v8::Module>> module_handle;
	std::shared_ptr<RemoteHandle<v8::Context>> context_handle;
	std::shared_ptr<RemoteHandle<v8::Value>> global_namespace;
	std::unordered_map<std::string, std::shared_ptr<IsolatedModule>> resolutions;
private:
	// helper methods
	static v8::MaybeLocal<v8::Module> ResolveCallback(v8::Local<v8::Context>, v8::Local<v8::String>, v8::Local<v8::Module>); // we may be able to use a weak map so can the ResolveCallback use the referrer value
public:
	IsolatedModule(std::shared_ptr<IsolateHolder>, std::shared_ptr<RemoteHandle<v8::Module>>, std::vector<std::string>);

	// BasicLockable requirements
	void lock();
	void unlock();

	const std::vector<std::string> & GetDependencySpecifiers() const;

	void SetDependency(std::string, std::shared_ptr<IsolatedModule>);
	
	void Instantiate(std::shared_ptr<RemoteHandle<v8::Context>>);

	std::unique_ptr<Transferable> Evaluate(std::size_t);

	v8::Local<v8::Value> GetNamespace();
};

class ContextHandle;
class ModuleHandle : public TransferableHandle {
	public:
	//typedef std::map<std::string, std::shared_ptr<RemoteHandle<v8::Module>>> dependency_map_type;
	private:
		class ModuleHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<IsolatedModule> isolated_module;
			public:
				ModuleHandleTransferable(std::shared_ptr<IsolateHolder>, std::shared_ptr<IsolatedModule>);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<IsolatedModule> isolated_module; 
	public:
		ModuleHandle(std::shared_ptr<IsolateHolder>, std::shared_ptr<IsolatedModule>);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		v8::Local<v8::Value> GetDependencySpecifiers();
		
		v8::Local<v8::Value> SetDependency(v8::Local<v8::String>, ModuleHandle*);
		
		template <int async>
		v8::Local<v8::Value> Instantiate(ContextHandle*);	

		
		template <int async>
		v8::Local<v8::Value> Evaluate(v8::MaybeLocal<v8::Object>);

		v8::Local<v8::Value> GetNamespace();
		/*
		template <int async>
		v8::Local<v8::Value> GetModuleNamespace(ContextHandle*);

		v8::Local<v8::Value> Release();*/
};

} // namespace ivm
