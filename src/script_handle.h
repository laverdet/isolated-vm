#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "transferable_handle.h"
#include "context_handle.h"

#include <memory>

namespace ivm {
using namespace v8;

class ScriptHandle : public TransferableHandle {
	private:
		std::shared_ptr<ShareablePersistent<UnboundScript>> script;

		class ScriptHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareablePersistent<UnboundScript>> script;
			public:
				ScriptHandleTransferable(shared_ptr<ShareablePersistent<UnboundScript>>& script) : script(script) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ScriptHandle>(script);
				}
		};

	public:
		ScriptHandle(std::shared_ptr<ShareablePersistent<UnboundScript>>& script) : script(script) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Script", New, 0,
				"runSync", Method<ScriptHandle, &ScriptHandle::RunSync>, 1
			));
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Context is private");
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ScriptHandleTransferable>(script);
		}

		void RunSync(const FunctionCallbackInfo<Value>& args);
};

}
