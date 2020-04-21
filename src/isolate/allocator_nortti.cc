#include "allocator.h"
#include "environment.h"
#include <cstdlib>

using namespace v8;

namespace ivm {
namespace {

class ExternalMemoryHandle {
	public:
		ExternalMemoryHandle(Local<Object> local_handle, size_t size) :
				handle{Isolate::GetCurrent(), local_handle}, size{size} {
			handle.SetWeak(reinterpret_cast<void*>(this), &WeakCallbackV8, WeakCallbackType::kParameter);
			IsolateEnvironment::GetCurrent()->AddWeakCallback(&handle, WeakCallback, this);
		}

		ExternalMemoryHandle(const ExternalMemoryHandle&) = delete;
		auto operator=(const ExternalMemoryHandle&) = delete;

		~ExternalMemoryHandle() {
			auto* allocator = IsolateEnvironment::GetCurrent()->GetLimitedAllocator();
			if (allocator != nullptr) {
				allocator->AdjustAllocatedSize(-size);
			}
		};

	private:
		static void WeakCallbackV8(const WeakCallbackInfo<void>& info) {
			WeakCallback(info.GetParameter());
		}

		static void WeakCallback(void* param) {
			auto* that = reinterpret_cast<ExternalMemoryHandle*>(param);
			IsolateEnvironment::GetCurrent()->RemoveWeakCallback(&that->handle);
			that->handle.Reset();
			delete that;
		}

		v8::Persistent<v8::Object> handle;
		size_t size;
};

} // anonymous namespace

/**
 * ArrayBuffer::Allocator that enforces memory limits. The v8 documentation specifically says
 * that it's unsafe to call back into v8 from this class but I took a look at
 * GetHeapStatistics() and I think it'll be ok.
 */
auto LimitedAllocator::Check(const size_t length) -> bool {
	if (v8_heap + env.extra_allocated_memory + length > next_check) {
		HeapStatistics heap_statistics;
		Isolate* isolate = Isolate::GetCurrent();
		isolate->GetHeapStatistics(&heap_statistics);
		v8_heap = heap_statistics.used_heap_size();
		if (v8_heap + env.extra_allocated_memory + length > limit + env.misc_memory_size) {
			// This is might be dangerous but the tests pass soooo..
			isolate->LowMemoryNotification();
			isolate->GetHeapStatistics(&heap_statistics);
			v8_heap = heap_statistics.used_heap_size();
			if (v8_heap + env.extra_allocated_memory + length > limit + env.misc_memory_size) {
				return false;
			}
		}
		next_check = v8_heap + env.extra_allocated_memory + length + 1024 * 1024;
	}
	return v8_heap + env.extra_allocated_memory + length <= limit + env.misc_memory_size;
}

LimitedAllocator::LimitedAllocator(IsolateEnvironment& env, size_t limit) : env(env), limit(limit), v8_heap(1024 * 1024 * 4), next_check(1024 * 1024) {}

auto LimitedAllocator::Allocate(size_t length) -> void* {
	if (Check(length)) {
		env.extra_allocated_memory += length;
		return std::calloc(length, 1);
	} else {
		++failures;
		if (length <= 64) { // kMinAddedElementsCapacity * sizeof(uint32_t)
			// When a tiny TypedArray is created v8 will avoid calling the allocator and instead just use
			// the internal heap. This is all fine until someone wants a pointer to the underlying buffer,
			// in that case v8 will "materialize" an ArrayBuffer which does invoke this allocator. If the
			// allocator refuses to return a valid pointer it will result in a hard crash so we have no
			// choice but to let this allocation succeed. Luckily the amount of memory allocated is tiny
			// and will soon be freed because at the same time we terminate the isolate.
			env.extra_allocated_memory += length;
			env.Terminate();
			return std::calloc(length, 1);
		} else {
			// The places end up here are more graceful and will throw a RangeError
			return nullptr;
		}
	}
}

auto LimitedAllocator::AllocateUninitialized(size_t length) -> void* {
	if (Check(length)) {
		env.extra_allocated_memory += length;
		return std::malloc(length);
	} else {
		++failures;
		if (length <= 64) {
			env.extra_allocated_memory += length;
			env.Terminate();
			return std::malloc(length);
		} else {
			return nullptr;
		}
	}
}

void LimitedAllocator::Free(void* data, size_t length) {
	env.extra_allocated_memory -= length;
	next_check -= length;
	std::free(data);
}

#ifdef USE_ALLOCATOR_REALLOCATE
auto LimitedAllocator::Reallocate(void* data, size_t old_length, size_t new_length) -> void* {
	auto delta = static_cast<ssize_t>(new_length) - static_cast<ssize_t>(old_length);
	if (delta > 0) {
		if (!Check(delta)) {
			return nullptr;
		}
	}
	env.extra_allocated_memory += delta;
	return ArrayBuffer::Allocator::Reallocate(data, old_length, new_length);
}
#endif

void LimitedAllocator::AdjustAllocatedSize(ptrdiff_t length) {
	env.extra_allocated_memory += length;
}

auto LimitedAllocator::GetFailureCount() const -> int {
	return failures;
}

void LimitedAllocator::Track(Local<Object> handle, size_t size) {
	new ExternalMemoryHandle{handle, size};
	AdjustAllocatedSize(size);
}

#if !V8_AT_LEAST(7, 9, 69)

// This attaches the life of a std::shared_ptr<BackingStore> to a v8 object so that the memory isn't
// freed while the object is still using it
class ArrayHolder {
	public:
		ArrayHolder(
			Local<Object> buffer,
			std::shared_ptr<BackingStore> backing_store) :
				handle{Isolate::GetCurrent(), buffer},
				backing_store{std::move(backing_store)} {

			handle.SetWeak(reinterpret_cast<void*>(this), &WeakCallbackV8, WeakCallbackType::kParameter);
			IsolateEnvironment::GetCurrent()->AddWeakCallback(&handle, WeakCallback, this);
			buffer->SetAlignedPointerInInternalField(0, this);
		}

		ArrayHolder(const ArrayHolder&) = delete;
		auto operator=(const ArrayHolder&) = delete;
		~ArrayHolder() = default;

		template <class Type>
		static auto Unwrap(Local<Type> handle) {
			auto ptr = reinterpret_cast<ArrayHolder*>(handle->GetAlignedPointerFromInternalField(0));
			if (ptr == nullptr || ptr->magic != ArrayHolder::kMagic) { // dangerous
				throw RuntimeGenericError("Array buffer is already externalized");
			}
			return ptr->backing_store;
		}

	private:
		static void WeakCallbackV8(const WeakCallbackInfo<void>& info) {
			WeakCallback(info.GetParameter());
		}

		static void WeakCallback(void* param) {
			auto that = reinterpret_cast<ArrayHolder*>(param);
			that->handle.Reset();
			IsolateEnvironment::GetCurrent()->RemoveWeakCallback(&that->handle);
			delete that;
		}

		static constexpr uint64_t kMagic = 0xa4d3c462f7fd1741;
		uint64_t magic = kMagic;
		v8::Persistent<v8::Object> handle;
		std::shared_ptr<BackingStore> backing_store;
};

template <class Type>
auto GetBackingStoreImpl(Type handle) -> std::shared_ptr<BackingStore> {
	size_t length = handle->ByteLength();
	if (length == 0) {
		throw RuntimeGenericError("Array buffer is invalid");
	}
	if (handle->IsExternal()) {
		// Buffer lifespan is not handled by v8.. attempt to recover from isolated-vm
		return ArrayHolder::Unwrap(handle);
	}

	// In this case the buffer is internal and can be released easily
	auto contents = handle->Externalize();
	auto data = std::unique_ptr<void, decltype(std::free)&>{contents.Data(), std::free};
	auto backing_store = std::make_shared<BackingStore>(std::move(data), length);
	new ArrayHolder{handle, backing_store};
	auto* allocator = IsolateEnvironment::GetCurrent()->GetLimitedAllocator();
	if (allocator != nullptr) {
		// Track this memory usage in the allocator
		new ExternalMemoryHandle{handle, length};
	}
	return backing_store;
}

auto BackingStore::GetBackingStore(v8::Local<v8::ArrayBuffer> handle) -> std::shared_ptr<BackingStore> {
	return GetBackingStoreImpl(handle);
}

auto BackingStore::GetBackingStore(v8::Local<v8::SharedArrayBuffer> handle) -> std::shared_ptr<BackingStore> {
	return GetBackingStoreImpl(handle);
}

auto BackingStore::NewArrayBuffer(std::shared_ptr<BackingStore> backing_store) -> v8::Local<v8::ArrayBuffer> {
	auto handle = ArrayBuffer::New(Isolate::GetCurrent(), backing_store->Data(), backing_store->ByteLength());
	new ArrayHolder{handle, std::move(backing_store)};
	return handle;
}

auto BackingStore::NewSharedArrayBuffer(std::shared_ptr<BackingStore> backing_store) -> v8::Local<v8::SharedArrayBuffer> {
	auto handle = SharedArrayBuffer::New(Isolate::GetCurrent(), backing_store->Data(), backing_store->ByteLength());
	new ArrayHolder{handle, std::move(backing_store)};
	return handle;
}

#endif

} // namespace ivm
