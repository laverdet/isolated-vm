export module napi_js:support.host;
import :value_handle;
import std;

namespace js::napi {

// Addressof operation
auto (*handle_addressof)(napi_value handle) noexcept -> void* = nullptr;

// Value inspection
auto (*maybe_fast_is_number_int32)(value_of<number_tag> value) -> std::optional<bool> = nullptr;
auto (*maybe_fast_is_string_one_byte)(value_of<string_tag> value) -> std::optional<bool> = nullptr;
auto (*maybe_is_shared_array_buffer)(napi_env env, value_of<object_tag> value) -> std::optional<bool> = nullptr;

// nb: These will crash on Bun
auto has_fast_is_functions = false;
auto fast_is_array(napi_value value) -> bool;
auto fast_is_bigint(napi_value value) -> bool;
auto fast_is_boolean(napi_value value) -> bool;
auto fast_is_function(napi_value value) -> bool;
auto fast_is_null(napi_value value) -> bool;
auto fast_is_number(napi_value value) -> bool;
auto fast_is_object(napi_value value) -> bool;
auto fast_is_string(napi_value value) -> bool;
auto fast_is_undefined(napi_value value) -> bool;

// 'SharedArrayBuffer' constructor
auto (*make_shared_array_buffer)(js::shared_array_buffer::shared_pointer_type data, std::size_t byte_length) -> value_of<shared_array_buffer_tag> = nullptr;

// 'ArrayBufferView' constructors
auto (*make_sab_data_view)(value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<data_view_tag> = nullptr;

template <class Type>
using make_sab_typed_array_constructor =
	auto(value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<typed_array_tag_of<Type>>;

template <class Type>
make_sab_typed_array_constructor<Type>* make_sab_typed_array_of;

template make_sab_typed_array_constructor<double>* make_sab_typed_array_of<double>;
template make_sab_typed_array_constructor<float>* make_sab_typed_array_of<float>;
template make_sab_typed_array_constructor<js::float16_t>* make_sab_typed_array_of<js::float16_t>;
template make_sab_typed_array_constructor<js::uint8_clamped_t>* make_sab_typed_array_of<js::uint8_clamped_t>;
template make_sab_typed_array_constructor<std::int16_t>* make_sab_typed_array_of<std::int16_t>;
template make_sab_typed_array_constructor<std::int32_t>* make_sab_typed_array_of<std::int32_t>;
template make_sab_typed_array_constructor<std::int64_t>* make_sab_typed_array_of<std::int64_t>;
template make_sab_typed_array_constructor<std::int8_t>* make_sab_typed_array_of<std::int8_t>;
template make_sab_typed_array_constructor<std::uint16_t>* make_sab_typed_array_of<std::uint16_t>;
template make_sab_typed_array_constructor<std::uint32_t>* make_sab_typed_array_of<std::uint32_t>;
template make_sab_typed_array_constructor<std::uint64_t>* make_sab_typed_array_of<std::uint64_t>;
template make_sab_typed_array_constructor<std::uint8_t>* make_sab_typed_array_of<std::uint8_t>;

// Once per process, this detects the host environment (nodejs vs bun)
auto initialize_host_environment(napi_env env) -> void;

} // namespace js::napi
