#pragma once
#include <node.h>
#include "isolate/class_handle.h"
#include <memory>

namespace ivm {

class TransferableHandle : public ClassHandle {
	public:
		static IsolateEnvironment::IsolateSpecific<FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<FunctionTemplate> tmpl;
			return tmpl;
		}

		static Local<FunctionTemplate> Definition() {
			return MakeClass("Transferable", nullptr, 0);
		}

		virtual std::unique_ptr<Transferable> TransferOut() = 0;
};

}
