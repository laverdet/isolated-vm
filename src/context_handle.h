#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ContextHandle : public TransferableHandle {
	friend struct RunRunner;
	friend class ModuleHandle;
	friend class NativeModuleHandle;
	private:
		RemoteHandle<v8::Context> context;
		RemoteHandle<v8::Value> global;
		RemoteHandle<v8::Object> global_reference;

		class ContextHandleTransferable : public Transferable {
			private:
				RemoteHandle<v8::Context> context;
				RemoteHandle<v8::Value> global;
			public:
				ContextHandleTransferable(
					RemoteHandle<v8::Context> context,
					RemoteHandle<v8::Value> global
				);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		ContextHandle(
			RemoteHandle<v8::Context> context,
			RemoteHandle<v8::Value> global
		);
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
		void CheckDisposed();
		v8::Local<v8::Value> GlobalGetter();
		void GlobalSetter(v8::Local<v8::Value> value);
		v8::Local<v8::Value> Release();
};

} // namespace ivm
