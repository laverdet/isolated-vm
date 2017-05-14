#pragma once
#include <node.h>
#include <memory>

namespace ivm {

class Transferable {
	protected:
		Transferable() {}

	public:
		~Transferable() {}
		static std::unique_ptr<Transferable> TransferOut(const v8::Local<v8::Value>& value);
		virtual v8::Local<v8::Value> TransferIn() = 0;
};

}
