#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "isolate/remote_handle.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ContextHandle;

class ScriptHandle : public TransferableHandle {
	private:
		class ScriptHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<RemoteHandle<v8::UnboundScript>> script;
			public:
				ScriptHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<RemoteHandle<v8::UnboundScript>> script
				);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<RemoteHandle<v8::UnboundScript>> script;

	public:
		ScriptHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<RemoteHandle<v8::UnboundScript>> script
		);

		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		template <int async>
		v8::Local<v8::Value> Run(ContextHandle* context_handle, v8::MaybeLocal<v8::Object> maybe_options);

		v8::Local<v8::Value> Release();
};

} // namespace ivm
