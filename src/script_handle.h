#pragma once
#include <node.h>
#include "shareable_isolate.h"
#include "shareable_persistent.h"
#include "class_handle.h"
#include "context_handle.h"

#include <memory>

namespace ivm {

class ScriptHandle : public ClassHandle {
	private:
		std::shared_ptr<ShareablePersistent<v8::UnboundScript>> script;

	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass(
				"Script", New, 0,
				"runSync", Method<ScriptHandle, &ScriptHandle::RunSync>, 1
			);
		}

		ScriptHandle(std::shared_ptr<ShareablePersistent<v8::UnboundScript>>& script) : script(script) {}

		static void New(const v8::FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Context is private");
		}

		void RunSync(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}
