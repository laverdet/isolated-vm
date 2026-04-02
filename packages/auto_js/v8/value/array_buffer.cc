module;
#include <memory>
#include <span>
module v8_js;
import :array_buffer;

namespace js::iv8 {

// `array_buffer`
array_buffer::operator js::array_buffer() const {
	return js::array_buffer{std::span<std::byte>{*this}};
}

array_buffer::operator std::span<std::byte>() const {
	return std::span<std::byte>{reinterpret_cast<std::byte*>((*this)->Data()), (*this)->ByteLength()};
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

} // namespace js::iv8
