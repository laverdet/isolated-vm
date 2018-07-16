#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>
#include <map>

namespace ivm {

class ContextHandle;

class ModuleHandle : public TransferableHandle {
	public:
	typedef std::map<std::string, std::shared_ptr<RemoteHandle<v8::Module>>> dependency_map_type;
	private:
		class ModuleHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<RemoteHandle<v8::Module>> _module;
			public:
				ModuleHandleTransferable(std::shared_ptr<IsolateHolder>, std::shared_ptr<RemoteHandle<v8::Module>>);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<RemoteHandle<v8::Module>> _module;
		std::shared_ptr<dependency_map_type> dependencies;
	public:
		ModuleHandle(std::shared_ptr<IsolateHolder>,std::shared_ptr<RemoteHandle<v8::Module>>);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		v8::Local<v8::Value> GetModuleRequestsLength();

		template <int async>
		v8::Local<v8::Value> GetModuleRequest(v8::Local<v8::Value>);

		v8::Local<v8::Value> SetDependency(v8::Local<v8::Value>, ModuleHandle*);

		template <int async>
		v8::Local<v8::Value> Instantiate(ContextHandle*);	

		template <int async>
		v8::Local<v8::Value> Evaluate(ContextHandle*, v8::MaybeLocal<v8::Object>);

		template <int async>
		v8::Local<v8::Value> GetModuleNamespace(ContextHandle*);

		v8::Local<v8::Value> Release();
};

} // namespace ivm
