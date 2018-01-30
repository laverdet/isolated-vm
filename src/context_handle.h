#pragma once
#include <node.h>
#include "isolate/environment.h"
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
		shared_ptr<IsolateHolder> isolate;
		shared_ptr<Persistent<Context>> context;
		shared_ptr<Persistent<Value>> global;

		class ContextHandleTransferable : public Transferable {
			private:
				shared_ptr<IsolateHolder> isolate;
				shared_ptr<Persistent<Context>> context;
				shared_ptr<Persistent<Value>> global;
			public:
				ContextHandleTransferable(
					shared_ptr<IsolateHolder> isolate,
					shared_ptr<Persistent<Context>>& context,
					shared_ptr<Persistent<Value>>& global
				) : isolate(isolate), context(context), global(global) {}
				virtual Local<Value> TransferIn() {
					return ClassHandle::NewInstance<ContextHandle>(isolate, context, global);
				}
		};

	public:
		ContextHandle(
			shared_ptr<IsolateHolder> isolate,
			shared_ptr<Persistent<Context>> context,
			shared_ptr<Persistent<Value>> global
		) : isolate(isolate), context(context), global(global) {}

		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return Inherit<TransferableHandle>(MakeClass(
				"Context", nullptr, 0,
				"globalReference", Parameterize<decltype(&ContextHandle::GlobalReference), &ContextHandle::GlobalReference>, 0
			));
		}

		virtual unique_ptr<Transferable> TransferOut() {
			return std::make_unique<ContextHandleTransferable>(isolate, context, global);
		}

		Local<Value> GlobalReference() {
			return ClassHandle::NewInstance<ReferenceHandle>(isolate, global, context, ReferenceHandle::TypeOf::Object);
		}
};

}
