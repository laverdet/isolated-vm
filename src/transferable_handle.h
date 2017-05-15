#pragma once
#include <node.h>
#include "class_handle.h"
#include <memory>

namespace ivm {

class TransferableHandle : public ClassHandle {
	public:
		static ShareableIsolate::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static ShareableIsolate::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static void New(const FunctionCallbackInfo<Value>& args) {
			THROW(Exception::TypeError, "Constructor Transferable is private");
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass("Transferable", New, 0);
		}

		~TransferableHandle() {}
		virtual std::unique_ptr<Transferable> TransferOut() = 0;
};

}
