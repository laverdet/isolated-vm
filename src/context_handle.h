#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ContextHandle : public TransferableHandle {
	friend class ScriptHandle;
	friend class NativeModuleHandle;
	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<v8::Persistent<v8::Context>> context;
		std::shared_ptr<v8::Persistent<v8::Value>> global;

		class ContextHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<v8::Persistent<v8::Context>> context;
				std::shared_ptr<v8::Persistent<v8::Value>> global;
			public:
				ContextHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<v8::Persistent<v8::Context>> context,
					std::shared_ptr<v8::Persistent<v8::Value>> global
				);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		ContextHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<v8::Persistent<v8::Context>> context,
			std::shared_ptr<v8::Persistent<v8::Value>> global
		);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
		v8::Local<v8::Value> GlobalReference();
};

} // namespace ivm
