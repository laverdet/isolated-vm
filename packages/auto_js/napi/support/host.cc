module napi_js;
import std;
import v8;

namespace js::napi {
util::lockable_with<bool, {.shared = true}> host_environment_initialized;

// Addressof operations
constexpr auto direct_handle_address = [](napi_value value) noexcept -> void* { return value; };
constexpr auto indirect_handle_address = [](napi_value value) noexcept -> void* { return *reinterpret_cast<void**>(value); };

// Fast functions which are supported on all platforms
// nb: Bun exports these v8 functions, but they crash with the `std::bit_cast<v8::Local<T>>` trick.
auto v8_is_false(napi_env /*env*/, napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsFalse();
}

auto v8_is_null(napi_env /*env*/, napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsNull();
}

auto v8_is_true(napi_env /*env*/, napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsTrue();
}

auto v8_is_undefined(napi_env /*env*/, napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsUndefined();
}

auto napi_fast_is_false(napi_env env, napi_value value) -> bool {
	return value == napi::invoke(napi_get_boolean, env, false);
}

auto napi_fast_is_null(napi_env env, napi_value value) -> bool {
	return value == napi::invoke(napi_get_null, env);
}

auto napi_fast_is_true(napi_env env, napi_value value) -> bool {
	return value == napi::invoke(napi_get_boolean, env, true);
}

auto napi_fast_is_undefined(napi_env env, napi_value value) -> bool {
	return value == napi::invoke(napi_get_undefined, env);
}

// Extended fast functions
// nb: Bun exports these v8 functions, but they crash with the `std::bit_cast<v8::Local<T>>` trick.
auto fast_is_array(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsArray();
}

auto fast_is_bigint(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsBigInt();
}

auto fast_is_function(napi_value value) -> bool {
	return std::bit_cast<v8::Local<v8::Value>>(value)->IsFunction();
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

// Bug fixes
auto napi_is_object_array_buffer(napi_env env, value_of<object_tag> value) -> bool {
	return napi::invoke(napi_is_arraybuffer, env, value);
}

auto napi_is_data_block_array_buffer(napi_env env, value_of<data_block_tag> value) -> bool {
	return napi_is_object_array_buffer(env, value);
}

auto bun_is_data_block_array_buffer(napi_env env, value_of<data_block_tag> value) -> bool {
	// https://github.com/oven-sh/bun/issues/32624
	auto* string = napi::invoke(napi_coerce_to_string, env, value);
	auto length = napi::invoke(napi_get_value_string_latin1, env, string, nullptr, 0);
	// '[object ArrayBuffer]'
	return length == 20;
}

auto bun_is_object_array_buffer(napi_env env, value_of<object_tag> value) -> bool {
	if (napi::invoke(napi_is_arraybuffer, env, value)) {
		return bun_is_data_block_array_buffer(env, value_of<data_block_tag>::from(value));
	} else {
		return false;
	}
}

// Optional value inspectors.
auto v8_is_number_int32(value_of<number_tag> value) -> std::optional<bool> {
	return std::bit_cast<v8::Local<v8::Number>>(value)->IsInt32();
};

auto v8_is_string_one_byte(value_of<string_tag> value) -> std::optional<bool> {
	return std::bit_cast<v8::Local<v8::String>>(value)->IsOneByte();
}

auto node_api_maybe_is_shared_array_buffer(napi_env env, value_of<object_tag> value) -> std::optional<bool> {
	return napi::invoke(node_api_is_sharedarraybuffer, env, value);
}

auto bun_maybe_is_shared_array_buffer(napi_env env, value_of<object_tag> value) -> std::optional<bool> {
	if (napi::invoke(napi_is_arraybuffer, env, value)) {
		return !bun_is_data_block_array_buffer(env, value_of<data_block_tag>::from(value));
	} else {
		return std::nullopt;
	}
}

constexpr auto unknown_maybe_is = [](auto...) -> std::optional<bool> { return std::nullopt; };

// 'SharedArrayBuffer' functions
auto v8_make_shared_array_buffer(js::shared_array_buffer::shared_pointer_type data, std::size_t byte_length) -> value_of<shared_array_buffer_tag> {
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

auto v8_shared_array_buffer_get_byte_length(value_of<shared_array_buffer_tag> buffer) -> std::size_t {
	return std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{buffer})->ByteLength();
};

auto v8_shared_array_buffer_get_backing_store(value_of<shared_array_buffer_tag> buffer) -> std::shared_ptr<std::byte[]> {
	auto backing_store = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(buffer)->GetBackingStore();
	auto* data = reinterpret_cast<std::byte*>(backing_store->Data());
	// NOLINTNEXTLINE(modernize-avoid-c-arrays)
	return std::shared_ptr<std::byte[]>{std::move(backing_store), data};
};

// 'ArrayBufferView' constructors
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto v8_make_sab_data_view(value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<data_view_tag> {
	auto buffer_local = std::bit_cast<v8::Local<v8::SharedArrayBuffer>>(napi_value{buffer});
	auto view_local = v8::DataView::New(buffer_local, byte_offset, length);
	return value_of<data_view_tag>::from(std::bit_cast<napi_value>(view_local));
};

template <class Type>
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto v8_make_sab_typed_array_of(value_of<shared_array_buffer_tag> buffer, std::size_t byte_offset, std::size_t length) -> value_of<typed_array_tag_of<Type>> {
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
// NOLINTNEXTLINE(performance-unnecessary-value-param)
constexpr auto unknown_sab_throw = [](auto...) -> Type {
	throw js::type_error{u"SharedArrayBuffer is not supported in this environment"};
};

// Dynamic libraries
constexpr auto node_uv_dlerror =
	[](const uv_lib_t* lib) -> const char* { return uv_dlerror(lib); };
constexpr auto node_uv_dlopen =
	[](const char* filename, uv_lib_t* lib) -> int { return uv_dlopen(filename, lib); };
constexpr auto node_uv_dlclose =
	[](uv_lib_t* lib) -> void { uv_dlclose(lib); };

constexpr auto unknown_uv_dlerror =
	[](const uv_lib_t* /*lib*/) -> const char* { return "Dynamic libraries are not supported in this environment"; };
constexpr auto unknown_uv_dlopen =
	[](const char* /*filename*/, uv_lib_t* /*lib*/) -> int { return -1; };
constexpr auto unknown_uv_dlclose =
	[](uv_lib_t* /*lib*/) -> void { std::unreachable(); };

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

			// Get `globalThis.process`
			auto* process_global = [ & ]() -> napi_value {
				auto* global = napi::invoke(napi_get_global, env);
				constexpr auto process_cw = util::make_consteval_string_view(util::cw<"process">);
				auto* process_str = napi::invoke(node_api_create_property_key_latin1, env, process_cw.data(), process_cw.length());
				auto* process = napi::invoke(napi_get_property, env, global, process_str);
				if (napi::invoke(napi_typeof, env, process) == napi_object) {
					return process;
				} else {
					return nullptr;
				}
			}();

			// Detect bun via `process.isBun`
			auto is_bun = process_global == nullptr ? false : [ & ]() -> bool {
				constexpr auto is_bun_cw = util::make_consteval_string_view(util::cw<"isBun">);
				auto* is_bun_str = napi::invoke(node_api_create_property_key_latin1, env, is_bun_cw.data(), is_bun_cw.length());
				auto* is_bun = napi::invoke(napi_get_property, env, process_global, is_bun_str);
				auto* is_bun_bool = napi::invoke(napi_coerce_to_bool, env, is_bun);
				return napi::invoke(napi_get_value_bool, env, is_bun_bool);
			}();

			// Detect deno via `process.versions.deno`
			auto is_deno = process_global == nullptr ? false : [ & ]() -> bool {
				constexpr auto versions_cw = util::make_consteval_string_view(util::cw<"versions">);
				auto* versions_str = napi::invoke(node_api_create_property_key_latin1, env, versions_cw.data(), versions_cw.length());
				auto* versions = napi::invoke(napi_get_property, env, process_global, versions_str);
				if (napi::invoke(napi_typeof, env, versions) != napi_object) {
					return false;
				}
				constexpr auto deno_cw = util::make_consteval_string_view(util::cw<"deno">);
				auto* deno_str = napi::invoke(node_api_create_property_key_latin1, env, deno_cw.data(), deno_cw.length());
				auto* deno = napi::invoke(napi_get_property, env, versions, deno_str);
				auto* deno_bool = napi::invoke(napi_coerce_to_bool, env, deno);
				return napi::invoke(napi_get_value_bool, env, deno_bool);
			}();

			auto is_nodejs = !is_bun && !is_deno;
			if (is_nodejs) {
				has_extended_fast_is_functions = true;
				fast_is_false = v8_is_false;
				fast_is_null = v8_is_null;
				fast_is_true = v8_is_true;
				fast_is_undefined = v8_is_undefined;
				host_uv_dlclose = node_uv_dlclose;
				host_uv_dlerror = node_uv_dlerror;
				host_uv_dlopen = node_uv_dlopen;
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
				maybe_is_number_int32 = v8_is_number_int32;
				maybe_is_shared_array_buffer = node_api_maybe_is_shared_array_buffer;
				maybe_is_string_latin1 = v8_is_string_one_byte;
				shared_array_buffer_get_backing_store = v8_shared_array_buffer_get_backing_store;
				shared_array_buffer_get_byte_length = v8_shared_array_buffer_get_byte_length;
			} else {
				has_extended_fast_is_functions = false;
				fast_is_false = napi_fast_is_false;
				fast_is_null = napi_fast_is_null;
				fast_is_true = napi_fast_is_true;
				fast_is_undefined = napi_fast_is_undefined;
				host_uv_dlclose = unknown_uv_dlclose;
				host_uv_dlerror = unknown_uv_dlerror;
				host_uv_dlopen = unknown_uv_dlopen;
				make_sab_data_view = unknown_sab_throw<value_of<data_view_tag>>;
				make_sab_typed_array_of<double> = unknown_sab_throw<value_of<typed_array_tag_of<double>>>;
				make_sab_typed_array_of<float> = unknown_sab_throw<value_of<typed_array_tag_of<float>>>;
				make_sab_typed_array_of<js::float16_t> = unknown_sab_throw<value_of<typed_array_tag_of<js::float16_t>>>;
				make_sab_typed_array_of<js::uint8_clamped_t> = unknown_sab_throw<value_of<typed_array_tag_of<js::uint8_clamped_t>>>;
				make_sab_typed_array_of<std::int16_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::int16_t>>>;
				make_sab_typed_array_of<std::int32_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::int32_t>>>;
				make_sab_typed_array_of<std::int64_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::int64_t>>>;
				make_sab_typed_array_of<std::int8_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::int8_t>>>;
				make_sab_typed_array_of<std::uint16_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::uint16_t>>>;
				make_sab_typed_array_of<std::uint32_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::uint32_t>>>;
				make_sab_typed_array_of<std::uint64_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::uint64_t>>>;
				make_sab_typed_array_of<std::uint8_t> = unknown_sab_throw<value_of<typed_array_tag_of<std::uint8_t>>>;
				make_shared_array_buffer = unknown_sab_throw<value_of<shared_array_buffer_tag>>;
				maybe_is_number_int32 = unknown_maybe_is;
				maybe_is_shared_array_buffer = unknown_maybe_is;
				maybe_is_string_latin1 = unknown_maybe_is;
				// NOLINTNEXTLINE(modernize-avoid-c-arrays)
				shared_array_buffer_get_backing_store = unknown_sab_throw<std::shared_ptr<std::byte[]>>;
				shared_array_buffer_get_byte_length = unknown_sab_throw<std::size_t>;
			}

			// Bug fixes
			is_data_block_array_buffer = napi_is_data_block_array_buffer;
			is_object_array_buffer = napi_is_object_array_buffer;
			if (is_bun) {
				is_data_block_array_buffer = bun_is_data_block_array_buffer;
				is_object_array_buffer = bun_is_object_array_buffer;
				maybe_is_shared_array_buffer = bun_maybe_is_shared_array_buffer;
			}
		}
	}
}

} // namespace js::napi
