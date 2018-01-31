#pragma once
#include <v8.h>
#include "isolate/class_handle.h"
#include "transferable.h"
#include <memory>

namespace ivm {

class TransferableHandle : public ClassHandle {
	public:
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific() {
			static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate> tmpl;
			return tmpl;
		}

		static v8::Local<v8::FunctionTemplate> Definition() {
			return MakeClass("Transferable", nullptr, 0);
		}

		virtual std::unique_ptr<Transferable> TransferOut() = 0;
};

} // namespace ivm
