#pragma once
#include <v8.h>
#include "isolate/class_handle.h"
#include "transferable.h"
#include <memory>

namespace ivm {

class TransferableHandle : public ClassHandle {
	public:
		static v8::Local<v8::FunctionTemplate> Definition() {
			return MakeClass("Transferable", nullptr);
		}

		virtual std::unique_ptr<Transferable> TransferOut() = 0;
};

} // namespace ivm
