module;
#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
module napi_js;
import :api;
import :array_buffer;
import v8;

namespace js::napi {

// `value_for_array_buffer`
auto value_for_array_buffer::make(const environment& env, const js::array_buffer& block) -> value<array_buffer_tag> {
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

// `bound_value_for_array_buffer`
bound_value_for_array_buffer::operator js::array_buffer() const {
	return js::array_buffer{std::span<std::byte>{*this}};
}

bound_value_for_array_buffer::operator std::span<std::byte>() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	void* bytes;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	size_t byte_length;
	napi::invoke0(napi_get_arraybuffer_info, env(), napi_value{*this}, &bytes, &byte_length);
	return std::span<std::byte>{reinterpret_cast<std::byte*>(bytes), byte_length};
}

// `value_for_shared_array_buffer`
auto value_for_shared_array_buffer::make(const environment& /*env*/, js::shared_array_buffer block) -> value<shared_array_buffer_tag> {
	auto byte_length = block.size();
	auto holder = std::make_unique<js::shared_array_buffer::shared_pointer_type>(std::move(block).acquire_ownership());
	auto backing_store = v8::SharedArrayBuffer::NewBackingStore(
		holder->get(),
		byte_length,
		[](void* /*data*/, size_t /*length*/, void* param) -> void {
			delete static_cast<js::shared_array_buffer::shared_pointer_type*>(param);
		},
		holder.get()
	);
	std::ignore = holder.release();
	auto shared_array_buffer = v8::SharedArrayBuffer::New(v8::Isolate::GetCurrent(), std::move(backing_store));
	return value<shared_array_buffer_tag>::from(std::bit_cast<napi_value>(shared_array_buffer));
}

// `bound_value_for_shared_array_buffer`
bound_value_for_shared_array_buffer::operator js::shared_array_buffer() const {
	throw std::logic_error{"unimplemented"};
}

// `value_for_typed_array`
auto value_for_typed_array::make(const environment& /*env*/, napi_typedarray_type type_tag, value<object_tag> buffer, size_t byte_offset, size_t length) -> value<object_tag> {
	const auto make = [ & ]<class Type>(std::type_identity<Type> /*type*/) -> value<object_tag> {
		auto buffer_local = std::bit_cast<v8::Local<v8::Object>>(napi_value{buffer});
		auto typed_array_local = [ & ]() -> v8::Local<Type> {
			if (buffer_local->IsSharedArrayBuffer()) {
				return Type::New(buffer_local.As<v8::SharedArrayBuffer>(), byte_offset, length);
			} else {
				return Type::New(buffer_local.As<v8::ArrayBuffer>(), byte_offset, length);
			}
		}();
		return value<object_tag>::from(std::bit_cast<napi_value>(typed_array_local));
	};
	switch (type_tag) {
		case napi_int8_array: return make(type<v8::Int8Array>);
		case napi_uint8_array: return make(type<v8::Uint8Array>);
		case napi_uint8_clamped_array: return make(type<v8::Uint8ClampedArray>);
		case napi_int16_array: return make(type<v8::Int16Array>);
		case napi_uint16_array: return make(type<v8::Uint16Array>);
		case napi_int32_array: return make(type<v8::Int32Array>);
		case napi_uint32_array: return make(type<v8::Uint32Array>);
		case napi_float32_array: return make(type<v8::Float32Array>);
		case napi_float64_array: return make(type<v8::Float64Array>);
		case napi_bigint64_array: return make(type<v8::BigInt64Array>);
		case napi_biguint64_array: return make(type<v8::BigUint64Array>);
	}

	// Imagine my shock when I found out `napi_create_typedarray` doesn't work with shared array buffers!
	// return value<object_tag>::from(napi::invoke(napi_create_typedarray, napi_env{env}, type, length, napi_value{buffer}, byte_offset));
}

// `bound_value_for_typed_array`
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
bound_value_for_typed_array::bound_value_for_typed_array(napi_env env, value<object_tag> typed_array) {
	napi::invoke0(napi_get_typedarray_info, env, napi_value{typed_array}, nullptr, &length_, nullptr, &array_buffer_, &byte_offset_);
}

// `value_for_data_view`
auto value_for_data_view::make(const environment& env, value<object_tag> buffer, size_t byte_offset, size_t length) -> value<data_view_tag> {
	return value<data_view_tag>::from(napi::invoke(napi_create_dataview, napi_env{env}, length, napi_value{buffer}, byte_offset));
}

// `bound_value_for_data_view`
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
bound_value_for_data_view::bound_value_for_data_view(napi_env env, value<data_view_tag> data_view) :
		bound_value_next{env, data_view} {
	napi::invoke0(napi_get_dataview_info, env, napi_value{data_view}, &length_, nullptr, &array_buffer_, &byte_offset_);
}

} // namespace js::napi
