#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>
#include <unordered_map>
#include <mutex>

namespace ivm {


class IsolatedModule {
public:
	class shared {
	private:
		using available_modules_type = std::unordered_multimap<int, IsolatedModule*>;
		static std::mutex mutex;
		static available_modules_type available_modules;
	public:
		// using raw pointers locks dangerous maybe, but couldn´t get it to work with weak_ptr.
		static IsolatedModule* find(v8::Local<v8::Module>);
		static void add(IsolatedModule*);
		static void remove(IsolatedModule*);
	};
private:
	std::recursive_mutex mutex;
	std::shared_ptr<IsolateHolder> isolate; // required by ClassHandle used to construct the namespace reference
	std::vector<std::string> dependency_specifiers;
	std::shared_ptr<RemoteHandle<v8::Module>> module_handle;
	std::shared_ptr<RemoteHandle<v8::Context>> context_handle;
	std::shared_ptr<RemoteHandle<v8::Value>> global_namespace;
	std::unordered_map<std::string, std::shared_ptr<IsolatedModule>> resolutions;
private:
	// helper methods
	static v8::MaybeLocal<v8::Module> ResolveCallback(v8::Local<v8::Context>, v8::Local<v8::String>, v8::Local<v8::Module>); // we may be able to use a weak map so can the ResolveCallback use the referrer value
public:
	IsolatedModule(std::shared_ptr<IsolateHolder>, std::shared_ptr<RemoteHandle<v8::Module>>, std::vector<std::string>);
	~IsolatedModule();
	
	// BasicLockable requirements
	void lock();
	void unlock();

	const std::vector<std::string> & GetDependencySpecifiers() const;

	void SetDependency(const std::string &, std::shared_ptr<IsolatedModule>);
	
	void Instantiate(std::shared_ptr<RemoteHandle<v8::Context>>);

	std::unique_ptr<Transferable> Evaluate(std::size_t);

	v8::Local<v8::Value> GetNamespace();

	friend bool operator==(const IsolatedModule&, const v8::Local<v8::Module>&);
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
