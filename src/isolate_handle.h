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
		static IsolateEnvironment::IsolateSpecific<v8::FunctionTemplate>& TemplateSpecific();
		static v8::Local<v8::FunctionTemplate> Definition();
		static std::unique_ptr<ClassHandle> New(v8::MaybeLocal<v8::Object> maybe_options);
		std::unique_ptr<Transferable> TransferOut() final;

		template <int async> v8::Local<v8::Value> CreateContext(v8::MaybeLocal<v8::Object> maybe_options);
		template <int async> v8::Local<v8::Value> CompileScript(v8::Local<v8::String> code_handle, v8::MaybeLocal<v8::Object> maybe_options);
		v8::Local<v8::Value> CreateInspectorSession();
		v8::Local<v8::Value> Dispose();
		template <int async> v8::Local<v8::Value> GetHeapStatistics();
		v8::Local<v8::Value> IsDisposedGetter();
		static v8::Local<v8::Value> CreateSnapshot(v8::Local<v8::Array> script_handles, v8::MaybeLocal<v8::String> warmup_handle);
};

class LimitedAllocator : public v8::ArrayBuffer::Allocator {
	private:
		size_t limit;
		size_t v8_heap;
		size_t my_heap;
		size_t next_check;
		int failures = 0;

		bool Check(const size_t length);

	public:
		explicit LimitedAllocator(size_t limit);
		void* Allocate(size_t length) final;
		void* AllocateUninitialized(size_t length) final;
		void Free(void* data, size_t length) final;
		size_t GetAllocatedSize() const;
		void AdjustAllocatedSize(ssize_t length);
		int GetFailureCount() const;
};

} // namespace ivm
