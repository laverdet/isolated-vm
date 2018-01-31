#pragma once
#include <v8.h>
#include "transferable_handle.h"
#include <memory>

namespace ivm {

/**
 * Reference to a v8 isolate
 */
class IsolateHandle : public TransferableHandle {
	private:
		std::shared_ptr<IsolateHolder> isolate;

		class IsolateHandleTransferable : public Transferable {
			private:
				std::shared_ptr<IsolateHolder> isolate;
			public:
				IsolateHandleTransferable(std::shared_ptr<IsolateHolder> isolate);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		IsolateHandle(std::shared_ptr<IsolateHolder> isolate);
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		static std::unique_ptr<ClassHandle> New(v8::MaybeLocal<v8::Object> maybe_options);
		std::unique_ptr<Transferable> TransferOut() final;

		template <bool async> v8::Local<v8::Value> CreateContext();
		template <bool async> v8::Local<v8::Value> CompileScript(v8::Local<v8::String> code_handle, v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> Dispose();
		v8::Local<v8::Value> GetHeapStatistics();
		static v8::Local<v8::Value> CreateSnapshot(v8::Local<v8::Array> script_handles, v8::MaybeLocal<v8::String> warmup_handle);
};

} // namespace ivm
