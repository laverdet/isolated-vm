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

class ContextHandle : public ClassHandle {
	friend class ScriptHandle;
	private:
		std::shared_ptr<ShareablePersistent<Context>> context;
		std::shared_ptr<ShareablePersistent<Value>> global;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass(
				"Context", New, 0,
				"globalReference", Method<ContextHandle, &ContextHandle::GlobalReference>, 0
			);
		}

		ContextHandle(std::shared_ptr<ShareablePersistent<Context>>& context, std::shared_ptr<ShareablePersistent<Value>>& global) : context(context), global(global) {}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Context is private");
		}

		void GlobalReference(const FunctionCallbackInfo<Value>& args) {
			args.GetReturnValue().Set(ClassHandle::NewInstance<ReferenceHandle>(global, context));
		}
};

}
