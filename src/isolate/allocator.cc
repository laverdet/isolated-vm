#include "allocator.h"

using namespace v8;

namespace ivm {

/**
 * ArrayBuffer::Allocator that enforces memory limits. The v8 documentation specifically says
 * that it's unsafe to call back into v8 from this class but I took a look at
 * GetHeapStatistics() and I think it'll be ok.
 */
bool LimitedAllocator::Check(const size_t length) {
	if (v8_heap + my_heap + length > next_check) {
		HeapStatistics heap_statistics;
		Isolate::GetCurrent()->GetHeapStatistics(&heap_statistics);
		v8_heap = heap_statistics.total_heap_size();
		if (v8_heap + my_heap + length > limit) {
			return false;
		}
		next_check = v8_heap + my_heap + length + 1024 * 1024;
	}
	if (v8_heap + my_heap + length > limit) {
		return false;
	}
	my_heap += length;
	return true;
}

LimitedAllocator::LimitedAllocator(size_t limit) : limit(limit), v8_heap(1024 * 1024 * 4), my_heap(0), next_check(1024 * 1024) {}

void* LimitedAllocator::Allocate(size_t length) {
	if (Check(length)) {
		return calloc(length, 1);
	} else {
		++failures;
		return nullptr;
	}
}

void* LimitedAllocator::AllocateUninitialized(size_t length) {
	if (Check(length)) {
		return malloc(length);
	} else {
		++failures;
		return nullptr;
	}
}

void LimitedAllocator::Free(void* data, size_t length) {
	my_heap -= length;
	next_check -= length;
	free(data);
}

size_t LimitedAllocator::GetAllocatedSize() const {
	return my_heap;
}

void LimitedAllocator::AdjustAllocatedSize(ssize_t length) {
	my_heap += length;
}

int LimitedAllocator::GetFailureCount() const {
	return failures;
}

} // namespace ivm
