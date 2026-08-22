module napi_js;
import std;
import v8;

namespace js::napi {

// `value_for_data_block`
auto value_for_data_block::detach() const -> void {
	napi::invoke0(napi_detach_arraybuffer, env(), napi_value{*this});
}

value_for_data_block::operator std::span<std::byte>() const {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	void* bytes;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::size_t byte_length;
	napi::invoke0(napi_get_arraybuffer_info, env(), napi_value{*this}, &bytes, &byte_length);
	return std::span{reinterpret_cast<std::byte*>(bytes), byte_length};
}

// `value_for_array_buffer`
value_for_array_buffer::operator js::array_buffer() const {
	return js::array_buffer{std::span<std::byte>{*this}};
}

// `value_for_shared_array_buffer`
value_for_shared_array_buffer::operator js::shared_array_buffer() const {
	auto byte_length = shared_array_buffer_get_byte_length(local_of{*this});
	auto backing_store = shared_array_buffer_get_backing_store(local_of{*this});
	return js::shared_array_buffer{byte_length, std::move(backing_store)};
}

// `local_for_typed_array`
auto local_for_typed_array::make(environment_lock_witness lock, napi_typedarray_type type_tag, local_of<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> local_of<typed_array_tag> {
	return local_of<typed_array_tag>::from(napi::invoke(napi_create_typedarray, napi_env{lock}, type_tag, length, napi_value{buffer}, byte_offset));
}

// `value_for_typed_array`
auto value_for_typed_array::make_bound(environment_lock_witness lock, local_of<typed_array_tag> typed_array) -> any_value_typed_array {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_typedarray_type type_tag;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_value array_buffer;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::size_t length;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::size_t byte_offset;
	napi::invoke0(napi_get_typedarray_info, napi_env{lock}, napi_value{typed_array}, &type_tag, &length, nullptr, &array_buffer, &byte_offset);
	const auto make = [ & ]<class Tag>(Tag /*tag*/) -> any_value_typed_array {
		return value_of<Tag>{lock, local_of<Tag>::from(typed_array), std::tuple{local_of<data_block_tag>::from(array_buffer), byte_offset, length}};
	};
	switch (type_tag) {
		case napi_bigint64_array: return make(typed_array_tag_of<std::int64_t>{});
		case napi_biguint64_array: return make(typed_array_tag_of<std::uint64_t>{});
		case napi_float16_array: return make(typed_array_tag_of<js::float16_t>{});
		case napi_float32_array: return make(typed_array_tag_of<float>{});
		case napi_float64_array: return make(typed_array_tag_of<double>{});
		case napi_int16_array: return make(typed_array_tag_of<std::int16_t>{});
		case napi_int32_array: return make(typed_array_tag_of<std::int32_t>{});
		case napi_int8_array: return make(typed_array_tag_of<std::int8_t>{});
		case napi_uint16_array: return make(typed_array_tag_of<std::uint16_t>{});
		case napi_uint32_array: return make(typed_array_tag_of<std::uint32_t>{});
		case napi_uint8_array: return make(typed_array_tag_of<std::uint8_t>{});
		case napi_uint8_clamped_array: return make(typed_array_tag_of<js::uint8_clamped_t>{});
	}
	std::unreachable();
}

// `local_for_data_view`
auto local_for_data_view::make(environment_lock_witness lock, local_of<data_block_tag> buffer, std::size_t byte_offset, std::size_t length) -> local_of<data_view_tag> {
	if (napi::invoke(napi_is_arraybuffer, napi_env{lock}, buffer)) {
		return make(lock, local_of<array_buffer_tag>::from(buffer), byte_offset, length);
	} else {
		return make(lock, local_of<shared_array_buffer_tag>::from(buffer), byte_offset, length);
	}
}

auto local_for_data_view::make(environment_lock_witness lock, local_of<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> local_of<data_view_tag> {
	return local_of<data_view_tag>::from(napi::invoke(napi_create_dataview, napi_env{lock}, length, napi_value{buffer}, byte_offset));
}

auto local_for_data_view::make(environment_lock_witness /*lock*/, local_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> local_of<data_view_tag> {
	return make_sab_data_view(buffer, byte_offset, length);
}

// `value_for_data_view`
value_for_data_view::value_for_data_view(environment_lock_witness lock, local_of<data_view_tag> data_view) :
		value_next{
			lock,
			data_view,
			[ & ] -> auto {
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				napi_value array_buffer;
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				std::size_t length;
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				std::size_t byte_offset;
				napi::invoke0(napi_get_dataview_info, napi_env{lock}, napi_value{data_view}, &length, nullptr, &array_buffer, &byte_offset);
				return std::tuple{local_of<data_block_tag>::from(array_buffer), byte_offset, length};
			}(),
		} {}

} // namespace js::napi
