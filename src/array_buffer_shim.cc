#include <v8.h>
#include <cassert>

// This should hopefully be temporary, these will be pure virtual once Blink implements them
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

void* ArrayBuffer::Allocator::Reserve(size_t length) {
	assert(false);
	return nullptr;
}

void ArrayBuffer::Allocator::SetProtection(void* data, size_t length, ArrayBuffer::Allocator::Protection protection) {
	assert(false);
}
}
