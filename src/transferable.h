#pragma once
#include <node.h>
#include <memory>

namespace ivm {
using namespace v8;

class Transferable {
	protected:
		Transferable() {}

	public:
		~Transferable() {}
		static std::unique_ptr<Transferable> TransferOut(const Local<Value>& value);
		virtual Local<Value> TransferIn() = 0;
};

}
