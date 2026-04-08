module v8_js;
import :array_buffer;
import std;

namespace js::iv8 {

// `array_buffer`
array_buffer::operator js::array_buffer() const {
	return js::array_buffer{std::span<std::byte>{*this}};
}

array_buffer::operator std::span<std::byte>() const {
	return std::span<std::byte>{reinterpret_cast<std::byte*>((*this)->Data()), (*this)->ByteLength()};
}

auto array_buffer::make(isolate_lock_witness lock, js::array_buffer block) -> v8::Local<v8::ArrayBuffer> {
	auto byte_length = block.size();
	auto backing_store = v8::ArrayBuffer::NewBackingStore(
		std::move(block).acquire_ownership().release(),
		byte_length,
		[](void* data, std::size_t /*length*/, void* /*deleter_data*/) -> void {
			std::ignore = js::array_buffer::unique_pointer_type{reinterpret_cast<std::byte*>(data)};
		},
		nullptr
	);
	return v8::ArrayBuffer::New(lock.isolate(), std::move(backing_store));
}

// `shared_array_buffer`
shared_array_buffer::operator js::shared_array_buffer() const {
	auto backing_store = (*this)->GetBackingStore();
	auto* data = reinterpret_cast<std::byte*>(backing_store->Data());
	auto data_shared_ptr = std::shared_ptr<js::shared_array_buffer::array_type>{std::move(backing_store), data};
	return js::shared_array_buffer{(*this)->ByteLength(), std::move(data_shared_ptr)};
}

shared_array_buffer::operator std::span<std::byte>() const {
	return std::span<std::byte>{reinterpret_cast<std::byte*>((*this)->Data()), (*this)->ByteLength()};
}

auto shared_array_buffer::make(isolate_lock_witness lock, js::shared_array_buffer block) -> v8::Local<v8::SharedArrayBuffer> {
	auto byte_length = block.size();
	auto backing_store = [ & ]() -> auto {
		if (byte_length == 0) {
			// v8 does not call the deleter `byte_length` is zero. So the heap-allocated shared_ptr trick
			// does not work in that case.
			return v8::SharedArrayBuffer::NewBackingStore(nullptr, 0, nullptr, nullptr);
		} else {
			auto holder = std::make_unique<js::shared_array_buffer::shared_pointer_type>(std::move(block).acquire_ownership());
			auto backing_store = v8::SharedArrayBuffer::NewBackingStore(
				holder->get(),
				byte_length,
				[](void* /*data*/, std::size_t /*length*/, void* param) -> void {
					delete static_cast<js::shared_array_buffer::shared_pointer_type*>(param);
				},
				holder.get()
			);
			std::ignore = holder.release();
			return backing_store;
		}
	}();
	return v8::SharedArrayBuffer::New(lock.isolate(), std::move(backing_store));
}

} // namespace js::iv8
