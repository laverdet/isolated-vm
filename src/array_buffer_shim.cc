// These shims are no longer necessary as of v8 version 6.7.9; commit 9568cea8
// The change was backported to nodejs v10.0.0 in a rolled up commit 2a3f8c3a
#include "isolate/v8_version.h"
#if defined(NODE_MODULE_VERSION) ? NODE_MODULE_VERSION < 64 : !V8_AT_LEAST(6, 7, 9)
#include <cassert>

namespace v8 {

void ArrayBuffer::Allocator::Free(void* data, size_t length, ArrayBuffer::Allocator::AllocationMode mode) {
	switch (mode) {
		case AllocationMode::kNormal:
			Free(data, length);
			return;
		case AllocationMode::kReservation:
			assert(false);
			return;
	}
}

void* ArrayBuffer::Allocator::Reserve(size_t /* length */) {
	assert(false);
	return nullptr;
}

void ArrayBuffer::Allocator::SetProtection(void* /* data */, size_t /* length */, ArrayBuffer::Allocator::Protection /* protection */) {
	assert(false);
}

} // namespace v8

#endif
