#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ContextHandle;

class ModuleHandle : public TransferableHandle {
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

	public:
		ModuleHandle(std::shared_ptr<IsolateHolder>,std::shared_ptr<RemoteHandle<v8::Module>>);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		//template <int async>
		v8::Local<v8::Value> Link(v8::Local<v8::Function>);

		v8::Local<v8::Value> Instantiate();

		//template <int async>
    v8::Local<v8::Value> Evaluate(v8::MaybeLocal<v8::Object>);

		v8::Local<v8::Value> Release();
};

} // namespace ivm
