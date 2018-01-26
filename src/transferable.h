#pragma once
#include <node.h>
#include <memory>

namespace ivm {
using namespace v8;

class Transferable {
	public:
		Transferable() = default;
		Transferable(const Transferable&) = delete;
		Transferable& operator= (const Transferable&) = delete;
		virtual ~Transferable() = default;
		static std::unique_ptr<Transferable> TransferOut(const Local<Value>& value);
		virtual Local<Value> TransferIn() = 0;
};

} // namespace ivm
