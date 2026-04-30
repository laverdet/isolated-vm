export module v8_js:array_buffer;
import :handle;
import :lock;
import :value.tag;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

class value_for_array_buffer : public handle_without_lock<v8::ArrayBuffer> {
	public:
		using handle_without_lock<v8::ArrayBuffer>::handle_without_lock;
		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

class value_for_shared_array_buffer : public handle_without_lock<v8::SharedArrayBuffer> {
	public:
		using handle_without_lock<v8::SharedArrayBuffer>::handle_without_lock;
		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::shared_array_buffer() const;
		explicit operator std::span<std::byte>() const;
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
