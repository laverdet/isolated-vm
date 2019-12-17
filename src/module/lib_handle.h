#pragma once
#include <v8.h>

#include <memory>

#include "transferable.h"
#include "transferable_handle.h"

namespace ivm {

class LibHandle : public TransferableHandle {
	private:
		class LibTransferable : public Transferable {
			public:
				v8::Local<v8::Value> TransferIn() final;
		};

		v8::Local<v8::Value> Hrtime(v8::MaybeLocal<v8::Array> maybe_diff);

	public:
		static v8::Local<v8::FunctionTemplate> Definition();
		std::unique_ptr<Transferable> TransferOut() final;
};

} // namespace ivm
