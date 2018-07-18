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
				explicit IsolateHandleTransferable(std::shared_ptr<IsolateHolder> isolate);
				v8::Local<v8::Value> TransferIn() final;
		};

	public:
		explicit IsolateHandle(std::shared_ptr<IsolateHolder> isolate);
		static v8::Local<v8::FunctionTemplate> Definition();
		static std::unique_ptr<ClassHandle> New(v8::MaybeLocal<v8::Object> maybe_options);
		std::unique_ptr<Transferable> TransferOut() final;

		template <int async> v8::Local<v8::Value> CreateContext(v8::MaybeLocal<v8::Object> maybe_options);
		template <int async> v8::Local<v8::Value> CompileScript(v8::Local<v8::String> code_handle, v8::MaybeLocal<v8::Object> maybe_options);
		template <int async> v8::Local<v8::Value> CompileModule(v8::Local<v8::String> code_handle, v8::MaybeLocal<v8::Object> maybe_options);
		
		v8::Local<v8::Value> CreateInspectorSession();
		v8::Local<v8::Value> Dispose();
		template <int async> v8::Local<v8::Value> GetHeapStatistics();
		v8::Local<v8::Value> GetCpuTime();
		v8::Local<v8::Value> GetWallTime();
		v8::Local<v8::Value> GetReferenceCount();
		v8::Local<v8::Value> IsDisposedGetter();
		static v8::Local<v8::Value> CreateSnapshot(v8::Local<v8::Array> script_handles, v8::MaybeLocal<v8::String> warmup_handle);
};

} // namespace ivm
