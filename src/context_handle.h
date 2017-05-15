#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "reference_handle.h"
#include "script_handle.h"

#include <memory>

namespace ivm {
using namespace v8;
using std::shared_ptr;

class ContextHandle : public TransferableHandle {
	friend class ScriptHandle;
	private:
		shared_ptr<ShareablePersistent<Context>> context;
		shared_ptr<ShareablePersistent<Value>> global;

		class ContextHandleTransferable : public Transferable {
			private:
				shared_ptr<ShareablePersistent<Context>> context;
				shared_ptr<ShareablePersistent<Value>> global;
			public:
				ContextHandleTransferable(shared_ptr<ShareablePersistent<Context>>& context, shared_ptr<ShareablePersistent<Value>>& global) : context(context), global(global) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ContextHandle>(context, global);
				}
		};

	public:
		ContextHandle(shared_ptr<ShareablePersistent<Context>>& context, shared_ptr<ShareablePersistent<Value>>& global) : context(context), global(global) {}

		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Context", New, 0,
				"globalReference", Method<ContextHandle, &ContextHandle::GlobalReference>, 0
			));
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Context is private");
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ContextHandleTransferable>(context, global);
		}

		void GlobalReference(const FunctionCallbackInfo<Value>& args) {
			args.GetReturnValue().Set(ClassHandle::NewInstance<ReferenceHandle>(global, context));
		}
};

}
