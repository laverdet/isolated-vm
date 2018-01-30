#pragma once
#include <node.h>
#include "isolate/environment.h"
#include "isolate/shareable_persistent.h"
#include "isolate/shareable_context.h"
#include "isolate/class_handle.h"
#include "reference_handle.h"

#include <memory>

namespace ivm {
using namespace v8;
using std::shared_ptr;

class ContextHandle : public TransferableHandle {
	friend class ScriptHandle;
	friend class NativeModuleHandle;
	private:
		shared_ptr<ShareableContext> context;
		shared_ptr<ShareablePersistent<Value>> global;

		class ContextHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareableContext> context;
				shared_ptr<ShareablePersistent<Value>> global;
			public:
				ContextHandleTransferable(shared_ptr<ShareableContext>& context, shared_ptr<ShareablePersistent<Value>>& global) : context(context), global(global) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ContextHandle>(context, global);
				}
		};

	public:
		ContextHandle(shared_ptr<ShareableContext> context, shared_ptr<ShareablePersistent<Value>> global) : context(context), global(global) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Context", nullptr, 0,
				"globalReference", Parameterize<decltype(&ContextHandle::GlobalReference), &ContextHandle::GlobalReference>, 0
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ContextHandleTransferable>(context, global);
		}

		Local<Value> GlobalReference() {
			return ClassHandle::NewInstance<ReferenceHandle>(global, context, ReferenceHandle::TypeOf::Object);
		}
};

}
