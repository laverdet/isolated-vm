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
				virtual v8::Local<v8::Value> TransferIn() override;
		};

		v8::Local<v8::Value> Hrtime(v8::MaybeLocal<v8::Array> maybe_diff);

	public:
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		virtual std::unique_ptr<Transferable> TransferOut() override;
};

}
