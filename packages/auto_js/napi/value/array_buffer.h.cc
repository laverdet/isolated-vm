export module napi_js:array_buffer;
import :bound_value;
import :object;
import :utility;
import :value_handle;
import std;

namespace js::napi {

// `ArrayBuffer`
class value_for_array_buffer : public value_next<array_buffer_tag> {
	public:
		static auto make(const environment& env, const js::array_buffer& block) -> value<array_buffer_tag>;
};

class bound_value_for_array_buffer : public bound_value_next<array_buffer_tag> {
	public:
		bound_value_for_array_buffer(napi_env env, value<array_buffer_tag> buffer) :
				bound_value_next{env, buffer} {}

		explicit operator js::array_buffer() const;
		explicit operator std::span<std::byte>() const;
};

// `SharedArrayBuffer`
class value_for_shared_array_buffer : public value_next<shared_array_buffer_tag> {
	public:
		static auto make(const environment& env, js::shared_array_buffer block) -> value<shared_array_buffer_tag>;
};

class bound_value_for_shared_array_buffer : public bound_value_next<shared_array_buffer_tag> {
	public:
		bound_value_for_shared_array_buffer(napi_env env, value<shared_array_buffer_tag> buffer) :
				bound_value_next{env, buffer} {}

		explicit operator js::shared_array_buffer() const;
};

// `TypedArray`
class value_for_typed_array {
	protected:
		static auto make(const environment& env, napi_typedarray_type type, value<object_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<object_tag>;
};

template <class Type>
class value_for_typed_array_of
		: public value_next<typed_array_tag_of<Type>>,
			public value_for_typed_array {
	public:
		static auto make(const environment& env, value<object_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<typed_array_tag_of<Type>> {
			constexpr auto get_array_buffer_type_tag = util::overloaded{
				[](std::type_identity<double>) -> napi_typedarray_type { return napi_float64_array; },
				[](std::type_identity<float>) -> napi_typedarray_type { return napi_float32_array; },
				[](std::type_identity<std::int16_t>) -> napi_typedarray_type { return napi_int16_array; },
				[](std::type_identity<std::int32_t>) -> napi_typedarray_type { return napi_int32_array; },
				[](std::type_identity<std::int64_t>) -> napi_typedarray_type { return napi_bigint64_array; },
				[](std::type_identity<std::int8_t>) -> napi_typedarray_type { return napi_int8_array; },
				[](std::type_identity<std::byte>) -> napi_typedarray_type { return napi_uint8_clamped_array; },
				[](std::type_identity<std::uint16_t>) -> napi_typedarray_type { return napi_uint16_array; },
				[](std::type_identity<std::uint32_t>) -> napi_typedarray_type { return napi_uint32_array; },
				[](std::type_identity<std::uint64_t>) -> napi_typedarray_type { return napi_biguint64_array; },
				[](std::type_identity<std::uint8_t>) -> napi_typedarray_type { return napi_uint8_array; },
			};
			auto type_tag = get_array_buffer_type_tag(type<Type>);
			return value<typed_array_tag_of<Type>>::from(value_for_typed_array::make(env, type_tag, buffer, byte_offset, length));
		}
};

class bound_value_for_typed_array : public bound_value_next<data_view_tag> {
	public:
		bound_value_for_typed_array(napi_env env, value<object_tag> typed_array);
		[[nodiscard]] auto buffer() const -> value<object_tag> { return value<object_tag>::from(array_buffer_); }
		[[nodiscard]] auto byte_offset() const -> std::size_t { return byte_offset_; }
		[[nodiscard]] auto size() const -> std::size_t { return length_; }

	private:
		napi_value array_buffer_;
		std::size_t byte_offset_;
		std::size_t length_;
};

template <class Type>
class bound_value_for_typed_array_of : public bound_value_for_typed_array {
	public:
		using bound_value_for_typed_array::bound_value_for_typed_array;
};

// `DataView`
class value_for_data_view : public value_next<data_view_tag> {
	public:
		static auto make(const environment& env, value<object_tag> buffer, std::size_t byte_offset, std::size_t length) -> value<data_view_tag>;
};

class bound_value_for_data_view : public bound_value_next<data_view_tag> {
	public:
		bound_value_for_data_view(napi_env env, value<data_view_tag> data_view);
		[[nodiscard]] auto buffer() const -> value<object_tag> { return value<object_tag>::from(array_buffer_); }
		[[nodiscard]] auto byte_offset() const -> std::size_t { return byte_offset_; }
		[[nodiscard]] auto size() const -> std::size_t { return length_; }

	private:
		napi_value array_buffer_;
		std::size_t byte_offset_;
		std::size_t length_;
};

} // namespace js::napi
