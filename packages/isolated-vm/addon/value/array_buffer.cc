module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.array_buffer;
import :handle.bound_value;
import :handle.value_of;
import :value.object;
import auto_js;
import std;

namespace isolated_vm {
using namespace js;

// value_of<data_block_tag>
class EXPORT value_for_data_block : public value_next<data_block_tag> {
	public:
		using value_next<data_block_tag>::value_next;
		[[nodiscard]] auto inspect() const -> value_typeof;
};

// bound_value<data_block_tag>
class EXPORT bound_value_for_data_block : public bound_value_next<data_block_tag> {
	public:
		using bound_value_next<data_block_tag>::bound_value_next;
		[[nodiscard]] auto byte_length() const -> std::size_t;
		[[nodiscard]] auto data() const -> std::byte*;
		explicit operator js::array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

// value_of<array_buffer_tag>
class EXPORT value_for_array_buffer : public value_next<array_buffer_tag> {
	public:
		using value_next<array_buffer_tag>::value_next;
		static auto make(const runtime_lock& lock, js::array_buffer buffer) -> value_of<array_buffer_tag>;
};

// value_of<array_buffer_view_tag>
class EXPORT value_for_array_buffer_view : public value_next<array_buffer_view_tag> {
	public:
		using value_next<array_buffer_view_tag>::value_next;
		[[nodiscard]] auto inspect() const -> value_typeof;
};

// bound_value<typed_array_tag_of<Type>>
template <class Type>
class EXPORT bound_value_for_typed_array_of : public bound_value_next<typed_array_tag_of<Type>> {
	public:
		using bound_value_next<typed_array_tag_of<Type>>::bound_value_next;
		[[nodiscard]] auto buffer() const -> value_of<data_block_tag>;
		[[nodiscard]] auto byte_offset() const -> std::size_t;
		[[nodiscard]] auto size() const -> std::size_t;
};

} // namespace isolated_vm
