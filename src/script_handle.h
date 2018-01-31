#pragma once
#include <v8.h>
#include "isolate/holder.h"
#include "transferable_handle.h"
#include <memory>

namespace ivm {

class ScriptHandle : public TransferableHandle {
	private:
		class ScriptHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
				std::shared_ptr<v8::Persistent<v8::UnboundScript>> script;
			public:
				ScriptHandleTransferable(
					std::shared_ptr<IsolateHolder> isolate,
					std::shared_ptr<v8::Persistent<v8::UnboundScript>> script
				);
				v8::Local<v8::Value> TransferIn() final;
		};

		std::shared_ptr<IsolateHolder> isolate;
		std::shared_ptr<v8::Persistent<v8::UnboundScript>> script;

	public:
		ScriptHandle(
			std::shared_ptr<IsolateHolder> isolate,
			std::shared_ptr<v8::Persistent<v8::UnboundScript>> script
		);

		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;

		template <bool async>
		v8::Local<v8::Value> Run(class ContextHandle* context_handle, v8::MaybeLocal<v8::Object> maybe_options);
};

} // namespace ivm
