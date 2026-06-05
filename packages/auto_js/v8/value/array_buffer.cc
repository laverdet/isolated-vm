module v8_js;
import :array_buffer;
import std;

namespace js::iv8 {

// `value_for_shared_array_buffer`
value_for_shared_array_buffer::operator js::shared_array_buffer() const {
	auto backing_store = (*this)->GetBackingStore();
	auto* data = reinterpret_cast<std::byte*>(backing_store->Data());
	auto data_shared_ptr = std::shared_ptr<js::shared_array_buffer::array_type>{std::move(backing_store), data};
	return js::shared_array_buffer{(*this)->ByteLength(), std::move(data_shared_ptr)};
}

} // namespace js::iv8
