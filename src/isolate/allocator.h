#pragma once
#include <v8.h>

namespace ivm {

class LimitedAllocator : public v8::ArrayBuffer::Allocator {
	private:
		class IsolateEnvironment& env;
		size_t limit;
		size_t v8_heap;
		size_t next_check;
		int failures = 0;

		bool Check(const size_t length);

	public:
		explicit LimitedAllocator(class IsolateEnvironment& env, size_t limit);
		void* Allocate(size_t length) final;
		void* AllocateUninitialized(size_t length) final;
		void Free(void* data, size_t length) final;
		// This is used by ExternalCopy when an ArrayBuffer is transferred. The memory is not freed but
		// we should no longer count it against the isolate
		void AdjustAllocatedSize(ssize_t length);
		int GetFailureCount() const;
};

} // namespace ivm
