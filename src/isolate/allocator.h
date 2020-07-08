#pragma once
#include <v8.h>
#include <memory>
#include "v8_version.h"

namespace ivm {

class LimitedAllocator : public v8::ArrayBuffer::Allocator {
	private:
		class IsolateEnvironment& env;
		size_t limit;
		size_t v8_heap;
		size_t next_check;
		int failures = 0;


	public:
		auto Check(size_t length) -> bool;
		explicit LimitedAllocator(class IsolateEnvironment& env, size_t limit);
		auto Allocate(size_t length) -> void* final;
		auto AllocateUninitialized(size_t length) -> void* final;
		void Free(void* data, size_t length) final;

#if NODE_MODULE_OR_V8_AT_LEAST(83, 8, 2, 1)
#define USE_ALLOCATOR_REALLOCATE
		// b5c917ee (v8) e8c7b7a2 (nodejs)
		auto Reallocate(void* data, size_t old_length, size_t new_length) -> void* final;
#else
#endif

		// This is used by ExternalCopy when an ArrayBuffer is transferred. The memory is not freed but
		// we should no longer count it against the isolate
		void AdjustAllocatedSize(ptrdiff_t length);
		auto GetFailureCount() const -> int;
		void Track(v8::Local<v8::Object> handle, size_t size);
};

#if V8_AT_LEAST(7, 9, 69)

using BackingStore = v8::BackingStore;

#else

class BackingStore {
	public:
		explicit BackingStore(size_t size) : data{std::malloc(size), std::free}, size{size} {}
		explicit BackingStore(std::unique_ptr<void, decltype(std::free)&> data, size_t size) : data{std::move(data)}, size{size} {}

		static auto GetBackingStore(v8::Local<v8::ArrayBuffer> handle) -> std::shared_ptr<BackingStore>;
		static auto GetBackingStore(v8::Local<v8::SharedArrayBuffer> handle) -> std::shared_ptr<BackingStore>;

		static auto NewArrayBuffer(std::shared_ptr<BackingStore> backing_store) -> v8::Local<v8::ArrayBuffer>;
		static auto NewSharedArrayBuffer(std::shared_ptr<BackingStore> backing_store) -> v8::Local<v8::SharedArrayBuffer>;

		auto ByteLength() const { return size; }
		auto Data() const { return data.get(); }

	private:
		std::unique_ptr<void, decltype(std::free)&> data;
		size_t size;
};

#endif

} // namespace ivm
