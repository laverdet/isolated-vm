#pragma once
#include <v8.h>

namespace ivm {

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
