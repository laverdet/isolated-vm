export module v8_js:array_buffer;
import :handle;
import :lock;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8 {

// `v8::Local<v8::ArrayBuffer>{}->Buffer()` returns a `v8::Local<v8::ArrayBuffer>`, even in the case
// it has a `SharedArrayBuffer`. In this case `buffer->IsArrayBuffer()` will return false. This is a
// wrapper to denote that there is a data block but we do not know what kind it is.
export class data_block : public v8::Local<v8::Object> {
	public:
		explicit data_block(v8::Local<v8::Object> handle) :
				v8::Local<v8::Object>{handle} {}
};

export class array_buffer : public v8::Local<v8::ArrayBuffer> {
	public:
		array_buffer() = default;
		explicit array_buffer(v8::Local<v8::ArrayBuffer> handle) :
				v8::Local<v8::ArrayBuffer>{handle} {}

		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

export class shared_array_buffer : public v8::Local<v8::SharedArrayBuffer> {
	public:
		shared_array_buffer() = default;
		explicit shared_array_buffer(v8::Local<v8::SharedArrayBuffer> handle) :
				v8::Local<v8::SharedArrayBuffer>{handle} {}

		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::shared_array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

export template <class Type>
class array_buffer_view : public v8::Local<Type> {
	public:
		array_buffer_view() = default;
		explicit array_buffer_view(v8::Local<Type> handle) :
				v8::Local<Type>{handle} {}

		[[nodiscard]] auto buffer() const -> iv8::data_block { return iv8::data_block{(*this)->Buffer()}; }

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
