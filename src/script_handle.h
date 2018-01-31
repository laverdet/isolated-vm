#pragma once
#include <node.h>
#include "isolate/holder.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ScriptHandle : public TransferableHandle {
	private:
		class ScriptHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<Persistent<UnboundScript>> script;
			public:
				ScriptHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<v8::Persistent<v8::UnboundScript>> script
				);
				Local<Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<v8::Persistent<v8::UnboundScript>> script;

	public:
		ScriptHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<v8::Persistent<v8::UnboundScript>> script
		);

		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific();
		static Local<FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		template <bool async>
		v8::Local<v8::Value> Run(class ContextHandle* context_handle, v8::MaybeLocal<v8::Object> maybe_options);
};

} // namespace ivm
