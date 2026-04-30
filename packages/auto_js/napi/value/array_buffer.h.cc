export module napi_js:array_buffer;
import :bound_value;
import :environment;
import :object;
import :utility;
import :value_handle;
import std;
import v8;

namespace js::napi {

// `ArrayBuffer`
class bound_value_for_array_buffer : public bound_value_next<array_buffer_tag> {
	public:
		using bound_value_next<array_buffer_tag>::bound_value_next;

		explicit operator js::array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

// `SharedArrayBuffer`
class bound_value_for_shared_array_buffer : public bound_value_next<shared_array_buffer_tag> {
	public:
		using bound_value_next<shared_array_buffer_tag>::bound_value_next;

		explicit operator js::shared_array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

// `ArrayBufferView` (hidden superclass of `TypedArray` and `DataView`)
class bound_value_for_array_buffer_view : public bound_value_next<array_buffer_view_tag> {
	protected:
		bound_value_for_array_buffer_view(
			napi_env env,
			value<array_buffer_view_tag> typed_array,
			std::tuple<value<data_block_tag>, std::size_t, std::size_t> array_buffer_info
		) :
				bound_value_next<array_buffer_view_tag>{env, typed_array},
				array_buffer_{std::get<0>(array_buffer_info)},
				byte_offset_{std::get<1>(array_buffer_info)},
				length_{std::get<2>(array_buffer_info)} {}

	public:
		[[nodiscard]] auto buffer() const -> value<data_block_tag> { return array_buffer_; }
		[[nodiscard]] auto byte_offset() const -> std::size_t { return byte_offset_; }
		[[nodiscard]] auto size() const -> std::size_t { return length_; }

	private:
		value<data_block_tag> array_buffer_;
		std::size_t byte_offset_;
		std::size_t length_;
};

// `TypedArray`
class value_for_typed_array : public value_next<typed_array_tag> {
	protected:
		static auto make(const environment& env, napi_typedarray_type type, value<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag>;
		static auto make(const environment& env, napi_typedarray_type type, value<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag>;
};

class bound_value_for_typed_array : public bound_value_next<typed_array_tag> {
	public:
		using bound_value_next<typed_array_tag>::bound_value_next;
		using any_bound_typed_array = std::variant<
			bound_value<typed_array_tag_of<double>>,
			bound_value<typed_array_tag_of<float>>,
			bound_value<typed_array_tag_of<std::byte>>,
			bound_value<typed_array_tag_of<std::int16_t>>,
			bound_value<typed_array_tag_of<std::int32_t>>,
			bound_value<typed_array_tag_of<std::int64_t>>,
			bound_value<typed_array_tag_of<std::int8_t>>,
			bound_value<typed_array_tag_of<std::uint16_t>>,
			bound_value<typed_array_tag_of<std::uint32_t>>,
			bound_value<typed_array_tag_of<std::uint64_t>>,
			bound_value<typed_array_tag_of<std::uint8_t>>>;

		static auto make_bound(const environment& env, value<typed_array_tag> typed_array) -> any_bound_typed_array;
};

template <class Type>
class value_for_typed_array_of : public value_next<typed_array_tag_of<Type>> {
	public:
		using value_next<typed_array_tag_of<Type>>::value_next;

		static auto make(const environment& env, value<data_block_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag_of<Type>> {
			if (napi::invoke(napi_is_arraybuffer, napi_env{env}, buffer)) {
				return make(env, napi::value<array_buffer_tag>::from(buffer), byte_offset, length);
			} else {
				return make(env, napi::value<shared_array_buffer_tag>::from(buffer), byte_offset, length);
			}
		}

		static auto make(const environment& env, value<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag_of<Type>> {
			constexpr auto type_tag = util::overloaded{
				[](std::type_identity<double>) -> napi_typedarray_type { return napi_float64_array; },
				[](std::type_identity<float>) -> napi_typedarray_type { return napi_float32_array; },
				[](std::type_identity<std::byte>) -> napi_typedarray_type { return napi_uint8_clamped_array; },
				[](std::type_identity<std::int16_t>) -> napi_typedarray_type { return napi_int16_array; },
				[](std::type_identity<std::int32_t>) -> napi_typedarray_type { return napi_int32_array; },
				[](std::type_identity<std::int64_t>) -> napi_typedarray_type { return napi_bigint64_array; },
				[](std::type_identity<std::int8_t>) -> napi_typedarray_type { return napi_int8_array; },
				[](std::type_identity<std::uint16_t>) -> napi_typedarray_type { return napi_uint16_array; },
				[](std::type_identity<std::uint32_t>) -> napi_typedarray_type { return napi_uint32_array; },
				[](std::type_identity<std::uint64_t>) -> napi_typedarray_type { return napi_biguint64_array; },
				[](std::type_identity<std::uint8_t>) -> napi_typedarray_type { return napi_uint8_array; },
			}(type<Type>);
			return value<typed_array_tag_of<Type>>::from(value_for_typed_array::make(env, type_tag, buffer, byte_offset, length));
		}

		// napi doesn't provide a way to make a typed array view on a shared array buffer
		static auto make(const environment& /*env*/, value<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag_of<Type>> {
			using constructor_type = type_t<util::overloaded{
				[](std::type_identity<double>) -> std::type_identity<v8::Float64Array> { return {}; },
				[](std::type_identity<float>) -> std::type_identity<v8::Float32Array> { return {}; },
				[](std::type_identity<std::byte>) -> std::type_identity<v8::Uint8ClampedArray> { return {}; },
				[](std::type_identity<std::int16_t>) -> std::type_identity<v8::Int16Array> { return {}; },
				[](std::type_identity<std::int32_t>) -> std::type_identity<v8::Int32Array> { return {}; },
				[](std::type_identity<std::int64_t>) -> std::type_identity<v8::BigInt64Array> { return {}; },
				[](std::type_identity<std::int8_t>) -> std::type_identity<v8::Int8Array> { return {}; },
				[](std::type_identity<std::uint16_t>) -> std::type_identity<v8::Uint16Array> { return {}; },
				[](std::type_identity<std::uint32_t>) -> std::type_identity<v8::Uint32Array> { return {}; },
				[](std::type_identity<std::uint64_t>) -> std::type_identity<v8::BigUint64Array> { return {}; },
				[](std::type_identity<std::uint8_t>) -> std::type_identity<v8::Uint8Array> { return {}; },
			}(type<Type>)>;
			auto buffer_local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{buffer});
			auto view_local = constructor_type::New(buffer_local, byte_offset, length);
			return value<typed_array_tag_of<Type>>::from(std::bit_cast<napi_value>(view_local));
		}
};

template <class Type>
class bound_value_for_typed_array_of : public bound_value_next<typed_array_tag_of<Type>> {
	public:
		using bound_value_next<typed_array_tag_of<Type>>::bound_value_next;
};

// `DataView`
class value_for_data_view : public value_next<data_view_tag> {
	public:
		using value_next<data_view_tag>::value_next;

		static auto make(const environment& env, value<data_block_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag>;
		static auto make(const environment& env, value<array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag>;
		static auto make(const environment& env, value<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag>;
};

class bound_value_for_data_view : public bound_value_next<data_view_tag> {
	public:
		bound_value_for_data_view(napi_env env, value<data_view_tag> data_view);
};

} // namespace js::napi
