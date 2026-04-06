export module v8_js:platform.allocator;
import std;
import v8;

namespace js::iv8::platform {

class data_block_allocator : public v8::ArrayBuffer::Allocator {
		auto Allocate(std::size_t length) -> void* final {
			auto* data = AllocateUninitialized(length);
			std::memset(data, 0, length);
			return data;
		};

		auto AllocateUninitialized(std::size_t length) -> void* final { return new std::byte[ length ]; };

		auto GetPageAllocator() -> v8::PageAllocator* final { return nullptr; }
		void Free(void* data, std::size_t /*length*/) final { delete[] static_cast<std::byte*>(data); };

		[[nodiscard]] auto MaxAllocationSize() const -> std::size_t final { return std::numeric_limits<std::uint32_t>::max(); }
};

} // namespace js::iv8::platform
