#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "transferable_handle.h"
#include "context_handle.h"
#include "external_copy.h"

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
				"Script", nullptr, 0,
				"run", Parameterize<decltype(&ScriptHandle::Run<true>), &ScriptHandle::Run<true>>, 1,
				"runSync", Parameterize<decltype(&ScriptHandle::Run<false>), &ScriptHandle::Run<false>>, 1
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ScriptHandleTransferable>(script);
		}

		template <bool async>
		Local<Value> Run(ContextHandle* context_handle) {
			return ThreePhaseRunner<async>(script->GetIsolate(), [this]() {
				return std::make_tuple();
			}, [this, context_handle]() {
				// Enter script's context and run it
				Local<Context> context = context_handle->context->Deref();
				Context::Scope context_scope(context);
				Local<Script> script = this->script->Deref()->BindToCurrentContext();
				Local<Value> result = Unmaybe(script->Run(context));
				return shared_ptr<Transferable>(ExternalCopy::CopyIfPrimitive(result));
			}, [](shared_ptr<Transferable> result) {
				if (result.get() == nullptr) {
					return Undefined(Isolate::GetCurrent()).As<Value>();
				} else {
					return result->TransferIn();
				}
			});
		}
};

}
