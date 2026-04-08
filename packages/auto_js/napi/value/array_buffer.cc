module napi_js;
import :api;
import :array_buffer;
import std;
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
	std::size_t byte_length;
	napi::invoke0(napi_get_arraybuffer_info, env(), napi_value{*this}, &bytes, &byte_length);
	return std::span<std::byte>{reinterpret_cast<std::byte*>(bytes), byte_length};
}

// `value_for_shared_array_buffer`
auto value_for_shared_array_buffer::make(const environment& /*env*/, js::shared_array_buffer block) -> value<shared_array_buffer_tag> {
	auto backing_store = [ & ]() -> auto {
		auto byte_length = block.size();
		// v8 does not call the deleter `byte_length` is zero. So the heap-allocated shared_ptr trick
		// does not work in that case.
		if (byte_length == 0) {
			return v8::SharedArrayBuffer::NewBackingStore(nullptr, 0, nullptr, nullptr);
		} else {
			auto holder = std::make_unique<js::shared_array_buffer::shared_pointer_type>(std::move(block).acquire_ownership());
			auto backing_store = v8::SharedArrayBuffer::NewBackingStore(
				holder->get(),
				byte_length,
				[](void* /*data*/, std::size_t /*length*/, void* param) -> void {
					delete static_cast<js::shared_array_buffer::shared_pointer_type*>(param);
				},
				holder.get()
			);
			std::ignore = holder.release();
			return backing_store;
		}
	}();
	auto shared_array_buffer = v8::SharedArrayBuffer::New(v8::Isolate::GetCurrent(), std::move(backing_store));
	return value<shared_array_buffer_tag>::from(std::bit_cast<napi_value>(shared_array_buffer));
}

// `bound_value_for_shared_array_buffer`
bound_value_for_shared_array_buffer::operator js::shared_array_buffer() const {
	auto local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{*this});
	auto backing_store = local->GetBackingStore();
	auto* data = reinterpret_cast<std::byte*>(backing_store->Data());
	auto data_shared_ptr = std::shared_ptr<js::shared_array_buffer::array_type>{std::move(backing_store), data};
	return js::shared_array_buffer{local->ByteLength(), std::move(data_shared_ptr)};
}

bound_value_for_shared_array_buffer::operator std::span<std::byte>() const {
	auto local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{*this});
	return std::span<std::byte>{reinterpret_cast<std::byte*>(local->Data()), local->ByteLength()};
}

// `value_for_typed_array`
auto value_for_typed_array::make(const environment& env, napi_typedarray_type type_tag, value<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag> {
	return value<typed_array_tag>::from(napi::invoke(napi_create_typedarray, napi_env{env}, type_tag, length, napi_value{buffer}, byte_offset));
}

// `bound_value_for_typed_array`
auto bound_value_for_typed_array::make_bound(const environment& env, value<typed_array_tag> typed_array) -> any_bound_typed_array {
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_typedarray_type type_tag;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_value array_buffer;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::size_t length;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	std::size_t byte_offset;
	napi::invoke0(napi_get_typedarray_info, env, napi_value{typed_array}, &type_tag, &length, nullptr, &array_buffer, &byte_offset);
	const auto make = [ & ]<class Tag>(Tag /*tag*/) -> any_bound_typed_array {
		return bound_value<Tag>{env, value<Tag>::from(typed_array), std::tuple{value<data_block_tag>::from(array_buffer), byte_offset, length}};
	};
	switch (type_tag) {
		case napi_bigint64_array: return make(typed_array_tag_of<std::int64_t>{});
		case napi_biguint64_array: return make(typed_array_tag_of<std::uint64_t>{});
		case napi_float32_array: return make(typed_array_tag_of<float>{});
		case napi_float64_array: return make(typed_array_tag_of<double>{});
		case napi_int16_array: return make(typed_array_tag_of<std::int16_t>{});
		case napi_int32_array: return make(typed_array_tag_of<std::int32_t>{});
		case napi_int8_array: return make(typed_array_tag_of<std::int8_t>{});
		case napi_uint16_array: return make(typed_array_tag_of<std::uint16_t>{});
		case napi_uint32_array: return make(typed_array_tag_of<std::uint32_t>{});
		case napi_uint8_array: return make(typed_array_tag_of<std::uint8_t>{});
		case napi_uint8_clamped_array: return make(typed_array_tag_of<std::byte>{});
	}
}

// `value_for_data_view`
auto value_for_data_view::make(const environment& env, value<data_block_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag> {
	if (napi::invoke(napi_is_arraybuffer, napi_env{env}, buffer)) {
		return make(env, napi::value<array_buffer_tag>::from(buffer), byte_offset, length);
	} else {
		return make(env, napi::value<shared_array_buffer_tag>::from(buffer), byte_offset, length);
	}
}

auto value_for_data_view::make(const environment& env, value<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag> {
	return value<data_view_tag>::from(napi::invoke(napi_create_dataview, napi_env{env}, length, napi_value{buffer}, byte_offset));
}

auto value_for_data_view::make(const environment& /*env*/, value<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag> {
	auto buffer_local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{buffer});
	auto view_local = v8::DataView::New(buffer_local, byte_offset, length);
	return value<data_view_tag>::from(std::bit_cast<napi_value>(view_local));
}

// `bound_value_for_data_view`
bound_value_for_data_view::bound_value_for_data_view(napi_env env, value<data_view_tag> data_view) :
		bound_value_next{
			env,
			data_view,
			[ & ] -> auto {
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				napi_value array_buffer;
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				std::size_t length;
				// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
				std::size_t byte_offset;
				napi::invoke0(napi_get_dataview_info, env, napi_value{data_view}, &length, nullptr, &array_buffer, &byte_offset);
				return std::tuple{value<data_block_tag>::from(array_buffer), byte_offset, length};
			}(),
		} {}

} // namespace js::napi
