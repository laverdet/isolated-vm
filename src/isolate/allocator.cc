#include "allocator.h"
#include "environment.h"

using namespace v8;

namespace ivm {

/**
 * ArrayBuffer::Allocator that enforces memory limits. The v8 documentation specifically says
 * that it's unsafe to call back into v8 from this class but I took a look at
 * GetHeapStatistics() and I think it'll be ok.
 */
bool LimitedAllocator::Check(const size_t length) {
	if (v8_heap + env.extra_allocated_memory + length > next_check) {
		HeapStatistics heap_statistics;
		Isolate::GetCurrent()->GetHeapStatistics(&heap_statistics);
		v8_heap = heap_statistics.total_heap_size();
		if (v8_heap + env.extra_allocated_memory + length > limit) {
			return false;
		}
		next_check = v8_heap + env.extra_allocated_memory + length + 1024 * 1024;
	}
	if (v8_heap + env.extra_allocated_memory + length > limit) {
		return false;
	}
	env.extra_allocated_memory += length;
	return true;
}

LimitedAllocator::LimitedAllocator(IsolateEnvironment& env, size_t limit) : env(env), limit(limit), v8_heap(1024 * 1024 * 4), next_check(1024 * 1024) {}

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
	env.extra_allocated_memory -= length;
	next_check -= length;
	free(data);
}

void LimitedAllocator::AdjustAllocatedSize(ptrdiff_t length) {
	env.extra_allocated_memory += length;
}

int LimitedAllocator::GetFailureCount() const {
	return failures;
}

} // namespace ivm
