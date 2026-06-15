module napi_js;
import std;
import v8;

namespace js::napi {
util::lockable_with<bool, {.shared = true}> host_environment_initialized;

// Addressof operations
constexpr auto direct_handle_address =
	[](napi_value value) noexcept -> void* { return value; };

constexpr auto indirect_handle_address =
	[](napi_value value) noexcept -> void* { return *reinterpret_cast<void**>(value); };

// Value inspection
constexpr auto v8_maybe_fast_is_number_int32 =
	[](value_of<number_tag> value) -> std::optional<bool> { return std::bit_cast<v8::Local<v8::Number>>(value)->IsInt32(); };

constexpr auto v8_maybe_fast_is_string_one_byte =
	[](value_of<string_tag> value) -> std::optional<bool> { return std::bit_cast<v8::Local<v8::String>>(value)->IsOneByte(); };

constexpr auto napi_maybe_is_shared_array_buffer =
	[](napi_env env, value_of<object_tag> value) -> std::optional<bool> { return napi::invoke(node_api_is_sharedarraybuffer, env, value); };

constexpr auto unknown_maybe_fast_is_number_int32 =
	[](value_of<number_tag> /*value*/) -> std::optional<bool> { return std::nullopt; };

constexpr auto unknown_maybe_fast_is_string_one_byte =
	[](value_of<string_tag> /*value*/) -> std::optional<bool> { return std::nullopt; };

constexpr auto unknown_maybe_is_shared_array_buffer =
	[](napi_env /*env*/, value_of<object_tag> /*value*/) -> std::optional<bool> { return std::nullopt; };

auto fast_is_array(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsArray();
}

auto fast_is_bigint(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsBigInt();
}

auto fast_is_boolean(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsBoolean();
}

auto fast_is_function(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsFunction();
}

auto fast_is_null(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsNull();
}

auto fast_is_number(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsNumber();
}

auto fast_is_object(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsObject();
}

auto fast_is_string(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsString();
}

auto fast_is_undefined(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsUndefined();
}

// 'SharedArrayBuffer' constructors
constexpr auto v8_make_shared_array_buffer =
	[](js::shared_array_buffer::shared_pointer_type data, std::size_t byte_length) -> value_of<shared_array_buffer_tag> {
	auto backing_store = [ & ]() -> auto {
		// v8 does not call the deleter `byte_length` is zero. So the heap-allocated shared_ptr trick
		// does not work in that case.
		if (byte_length == 0) {
			return v8::SharedArrayBuffer::NewBackingStore(nullptr, 0, nullptr, nullptr);
		} else {
			auto holder = std::make_unique<js::shared_array_buffer::shared_pointer_type>(std::move(data));
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
	return value_of<shared_array_buffer_tag>::from(std::bit_cast<napi_value>(shared_array_buffer));
};

constexpr auto unknown_make_shared_array_buffer =
	// NOLINTNEXTLINE(performance-unnecessary-value-param)
	[](js::shared_array_buffer::shared_pointer_type /*data*/, std::size_t /*byte_length*/) -> value_of<shared_array_buffer_tag> {
	throw js::type_error{u"SharedArrayBuffer is not supported in this environment"};
};

// 'ArrayBufferView' constructors
constexpr auto v8_make_sab_data_view =
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	[](value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<data_view_tag> {
	auto buffer_local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{buffer});
	auto view_local = v8::DataView::New(buffer_local, byte_offset, length);
	return value_of<data_view_tag>::from(std::bit_cast<napi_value>(view_local));
};

constexpr auto unknown_make_sab_data_view =
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	[](value_of<shared_array_buffer_tag> /*buffer*/, std::size_t /*byte_offset*/, std::size_t /*length*/) -> value_of<data_view_tag> {
	throw js::type_error{u"SharedArrayBuffer is not supported in this environment"};
};

template <class Type>
constexpr auto v8_make_sab_typed_array_of =
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	[](value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<typed_array_tag_of<Type>> {
	using constructor_type = type_t<util::overloaded{
		[](std::type_identity<double>) -> std::type_identity<v8::Float64Array> { return {}; },
		[](std::type_identity<float>) -> std::type_identity<v8::Float32Array> { return {}; },
		[](std::type_identity<js::float16_t>) -> std::type_identity<v8::Float16Array> { return {}; },
		[](std::type_identity<js::uint8_clamped_t>) -> std::type_identity<v8::Uint8ClampedArray> { return {}; },
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
	return value_of<typed_array_tag_of<Type>>::from(std::bit_cast<napi_value>(view_local));
};

template <class Type>
constexpr auto unknown_make_sab_typed_array_of =
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
	[](value_of<shared_array_buffer_tag> /*buffer*/, std::size_t /*byte_offset*/, std::size_t /*length*/) -> value_of<typed_array_tag_of<Type>> {
	throw js::type_error{u"SharedArrayBuffer is not supported in this environment"};
};

auto initialize_host_environment(napi_env env) -> void {
	if (!*host_environment_initialized.read()) {
		if (auto lock = host_environment_initialized.write(); !*lock) {
			// Detect indirect handles (nodejs, v8 [configurable]) or direct handles (bun, jsc)
			// See: `v8_enable_direct_handle`
			auto uses_direct_handles = [ & ] -> bool {
				auto* array = napi::invoke(napi_create_array_with_length, env, 1);
				napi::invoke0(napi_set_element, env, array, 0, array);
				auto* result = napi::invoke(napi_get_element, env, array, 0);
				if (direct_handle_address(array) == direct_handle_address(result)) {
					return true;
				} else if (indirect_handle_address(array) == indirect_handle_address(result)) {
					return false;
				} else {
					throw std::runtime_error{"Exotic napi handle behavior detected"};
				}
			}();
			*lock = true;
			if (uses_direct_handles) {
				handle_addressof = direct_handle_address;
			} else {
				handle_addressof = indirect_handle_address;
			}

			// Detect bun via `process.isBun`
			auto is_bun = [ & ] -> bool {
				auto* global = napi::invoke(napi_get_global, env);
				constexpr auto process_cw = util::make_consteval_string_view(util::cw<"process">);
				auto* process_str = napi::invoke(node_api_create_property_key_latin1, env, process_cw.data(), process_cw.length());
				auto* process = napi::invoke(napi_get_property, env, global, process_str);
				if (napi::invoke(napi_typeof, env, process) == napi_object) {
					constexpr auto is_bun_cw = util::make_consteval_string_view(util::cw<"isBun">);
					auto* is_bun_str = napi::invoke(node_api_create_property_key_latin1, env, is_bun_cw.data(), is_bun_cw.length());
					auto* is_bun = napi::invoke(napi_get_property, env, process, is_bun_str);
					auto* is_bun_bool = napi::invoke(napi_coerce_to_bool, env, is_bun);
					return napi::invoke(napi_get_value_bool, env, is_bun_bool);
				} else {
					return false;
				}
			}();
			if (is_bun) {
				has_fast_is_functions = false;
				make_sab_data_view = unknown_make_sab_data_view;
				make_sab_typed_array_of<double> = unknown_make_sab_typed_array_of<double>;
				make_sab_typed_array_of<float> = unknown_make_sab_typed_array_of<float>;
				make_sab_typed_array_of<js::float16_t> = unknown_make_sab_typed_array_of<js::float16_t>;
				make_sab_typed_array_of<js::uint8_clamped_t> = unknown_make_sab_typed_array_of<js::uint8_clamped_t>;
				make_sab_typed_array_of<std::int16_t> = unknown_make_sab_typed_array_of<std::int16_t>;
				make_sab_typed_array_of<std::int32_t> = unknown_make_sab_typed_array_of<std::int32_t>;
				make_sab_typed_array_of<std::int64_t> = unknown_make_sab_typed_array_of<std::int64_t>;
				make_sab_typed_array_of<std::int8_t> = unknown_make_sab_typed_array_of<std::int8_t>;
				make_sab_typed_array_of<std::uint16_t> = unknown_make_sab_typed_array_of<std::uint16_t>;
				make_sab_typed_array_of<std::uint32_t> = unknown_make_sab_typed_array_of<std::uint32_t>;
				make_sab_typed_array_of<std::uint64_t> = unknown_make_sab_typed_array_of<std::uint64_t>;
				make_sab_typed_array_of<std::uint8_t> = unknown_make_sab_typed_array_of<std::uint8_t>;
				make_shared_array_buffer = unknown_make_shared_array_buffer;
				maybe_fast_is_number_int32 = unknown_maybe_fast_is_number_int32;
				maybe_fast_is_string_one_byte = unknown_maybe_fast_is_string_one_byte;
				maybe_is_shared_array_buffer = unknown_maybe_is_shared_array_buffer;
			} else {
				has_fast_is_functions = true;
				make_sab_data_view = v8_make_sab_data_view;
				make_sab_typed_array_of<double> = v8_make_sab_typed_array_of<double>;
				make_sab_typed_array_of<float> = v8_make_sab_typed_array_of<float>;
				make_sab_typed_array_of<js::float16_t> = v8_make_sab_typed_array_of<js::float16_t>;
				make_sab_typed_array_of<js::uint8_clamped_t> = v8_make_sab_typed_array_of<js::uint8_clamped_t>;
				make_sab_typed_array_of<std::int16_t> = v8_make_sab_typed_array_of<std::int16_t>;
				make_sab_typed_array_of<std::int32_t> = v8_make_sab_typed_array_of<std::int32_t>;
				make_sab_typed_array_of<std::int64_t> = v8_make_sab_typed_array_of<std::int64_t>;
				make_sab_typed_array_of<std::int8_t> = v8_make_sab_typed_array_of<std::int8_t>;
				make_sab_typed_array_of<std::uint16_t> = v8_make_sab_typed_array_of<std::uint16_t>;
				make_sab_typed_array_of<std::uint32_t> = v8_make_sab_typed_array_of<std::uint32_t>;
				make_sab_typed_array_of<std::uint64_t> = v8_make_sab_typed_array_of<std::uint64_t>;
				make_sab_typed_array_of<std::uint8_t> = v8_make_sab_typed_array_of<std::uint8_t>;
				make_shared_array_buffer = v8_make_shared_array_buffer;
				maybe_fast_is_number_int32 = v8_maybe_fast_is_number_int32;
				maybe_fast_is_string_one_byte = v8_maybe_fast_is_string_one_byte;
				maybe_is_shared_array_buffer = napi_maybe_is_shared_array_buffer;
			}
		}
	}
}

} // namespace js::napi
