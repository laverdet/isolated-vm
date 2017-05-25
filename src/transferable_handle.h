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

		static Local<FunctionTemplate> Definition() {
			return MakeClass("Transferable", nullptr, 0);
		}

		virtual std::unique_ptr<Transferable> TransferOut() = 0;
};

}
