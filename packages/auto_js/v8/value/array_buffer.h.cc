export module v8_js:array_buffer;
import :handle;
import :value.tag;
import auto_js;
import std;
import v8;

namespace js::iv8 {

// nb: This handles `v8::ArrayBuffer` and also `v8::SharedArrayBuffer`. They aren't related by
// inheritance but v8 seem to allow it, and even encourage it.
class value_for_data_block : public handle_without_lock<v8::ArrayBuffer> {
	public:
		using handle_without_lock<v8::ArrayBuffer>::handle_without_lock;
		[[nodiscard]] auto byte_length() const -> std::size_t { return (*this)->ByteLength(); }
		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		explicit operator std::span<std::byte>() const { return {data(), byte_length()}; }
};

class value_for_array_buffer : public handle_without_lock<v8::ArrayBuffer> {
	public:
		using handle_without_lock<v8::ArrayBuffer>::handle_without_lock;
		[[nodiscard]] auto byte_length() const -> std::size_t { return (*this)->ByteLength(); }
		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		explicit operator js::array_buffer() const { return js::array_buffer{std::span<std::byte>{*this}}; }
		explicit operator std::span<std::byte>() const { return {data(), byte_length()}; }
};

class value_for_shared_array_buffer : public handle_without_lock<v8::SharedArrayBuffer> {
	public:
		using handle_without_lock<v8::SharedArrayBuffer>::handle_without_lock;
		[[nodiscard]] auto byte_length() const -> std::size_t { return (*this)->ByteLength(); }
		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		explicit operator js::shared_array_buffer() const;
		explicit operator std::span<std::byte>() const { return {data(), byte_length()}; }
};

template <class Type>
class value_for_array_buffer_view : public handle_without_lock<Type> {
	public:
		using handle_without_lock<Type>::handle_without_lock;

		[[nodiscard]] auto buffer() const -> v8::Local<iv8::DataBlock> {
			return (*this)->Buffer().template As<iv8::DataBlock>();
		}

		[[nodiscard]] auto byte_offset() const -> std::size_t { return (*this)->ByteOffset(); }

		[[nodiscard]] auto size() const -> std::size_t {
			if constexpr (type<Type> == type<v8::DataView>) {
				return (*this)->ByteLength();
			} else {
				return (*this)->Length();
			}
		}
};

} // namespace js::iv8
