module;
#include <span>
export module v8_js:array_buffer;
import :handle;
import :lock;
import auto_js;
import util;
import v8;

namespace js::iv8 {

export class array_buffer
		: public v8::Local<v8::ArrayBuffer>,
			public handle_with_isolate {
	public:
		array_buffer() = default;
		explicit array_buffer(isolate_lock_witness lock, v8::Local<v8::ArrayBuffer> handle) :
				v8::Local<v8::ArrayBuffer>{handle},
				handle_with_isolate{lock} {}

		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::data_block() const;
		explicit operator std::span<std::byte>() const;
};

export class shared_array_buffer
		: public v8::Local<v8::SharedArrayBuffer>,
			public handle_with_isolate {
	public:
		shared_array_buffer() = default;
		explicit shared_array_buffer(isolate_lock_witness lock, v8::Local<v8::SharedArrayBuffer> handle) :
				v8::Local<v8::SharedArrayBuffer>{handle},
				handle_with_isolate{lock} {}

		[[nodiscard]] auto data() const -> std::byte* { return static_cast<std::byte*>((*this)->Data()); }
		[[nodiscard]] auto size() const -> std::size_t { return (*this)->ByteLength(); }
		explicit operator js::data_block() const;
		explicit operator std::span<std::byte>() const;
};

} // namespace js::iv8
