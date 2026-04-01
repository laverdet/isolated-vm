module;
#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
module napi_js;
import :api;
import :array_buffer;
import v8;

namespace js::napi {

auto value_for_array_buffer::make(const environment& env, const js::data_block& block) -> value<array_buffer_tag> {
	// You could avoid the extra copy for `js::data_block&&` here w/ v8 API. Napi requires the copy
	// though.
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	void* bytes;
	auto* result = napi::invoke(napi_create_arraybuffer, napi_env{env}, block.size(), &bytes);
	// nb: `std::memcpy` *technically* results in undefined behavior on block size 0
	// (and also) it maybe causes an infinite loop with musl
	// https://stackoverflow.com/questions/5243012/is-it-guaranteed-to-be-safe-to-perform-memcpy0-0-0
	std::ranges::copy(std::span<const std::byte>{block}, static_cast<std::byte*>(bytes));
	return value<array_buffer_tag>::from(result);
}

bound_value_for_array_buffer::operator js::data_block() const {
	return js::data_block{std::span<std::byte>{*this}};
}

bound_value_for_array_buffer::operator std::span<std::byte>() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	void* bytes;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	size_t byte_length;
	napi::invoke0(napi_get_arraybuffer_info, env(), napi_value{*this}, &bytes, &byte_length);
	return std::span<std::byte>{reinterpret_cast<std::byte*>(bytes), byte_length};
}

auto value_for_shared_array_buffer::make(const environment& /*env*/, js::data_block block) -> value<shared_array_buffer_tag> {
	auto byte_length = block.size();
	auto block_pointer = std::move(block).acquire_ownership();
	if (block_pointer.index() == std::variant_npos) {
		std::unreachable();
	}
	auto backing_store = std::visit(
		util::overloaded{
			[ byte_length ](js::data_block::shared_pointer_type data) -> std::unique_ptr<v8::BackingStore> {
				auto holder = std::make_unique<js::data_block::shared_pointer_type>(std::move(data));
				auto backing_store = v8::SharedArrayBuffer::NewBackingStore(
					holder->get(),
					byte_length,
					[](void* /*data*/, size_t /*length*/, void* param) -> void {
						delete static_cast<js::data_block::shared_pointer_type*>(param);
					},
					holder.get()
				);
				std::ignore = holder.release();
				return backing_store;
			},
			[ byte_length ](js::data_block::unique_pointer_type data) -> std::unique_ptr<v8::BackingStore> {
				return v8::SharedArrayBuffer::NewBackingStore(
					data.release(),
					byte_length,
					[](void* data, size_t /*length*/, void* /*param*/) -> void {
						delete[] static_cast<js::data_block::unique_pointer_type::element_type*>(data);
					},
					nullptr
				);
			},
			[](const auto& /*wrong*/) -> std::unique_ptr<v8::BackingStore> {
				throw std::logic_error{"invalid data block pointer type"};
			},
		},
		std::move(block_pointer)
	);
	auto shared_array_buffer = v8::SharedArrayBuffer::New(v8::Isolate::GetCurrent(), std::move(backing_store));
	return value<shared_array_buffer_tag>::from(std::bit_cast<napi_value>(shared_array_buffer));
}

bound_value_for_shared_array_buffer::
operator js::data_block() const {
	throw std::logic_error{"unimplemented"};
}

} // namespace js::napi
