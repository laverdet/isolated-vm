module;
#include <js_native_api_types.h>
export module nodejs:js_native_api_types;
// NOLINTBEGIN(misc-unused-using-decls)

export using ::napi_env;
export using ::node_api_basic_env;

export using ::napi_callback_info;
export using ::napi_deferred;
export using ::napi_escapable_handle_scope;
export using ::napi_handle_scope;
export using ::napi_ref;
export using ::napi_value;

export using ::napi_callback;
export using ::napi_extended_error_info;
export using ::napi_finalize;
export using ::napi_property_descriptor;
export using ::node_api_basic_finalize;
export using ::node_api_nogc_finalize;

// napi_valuetype
export using ::napi_valuetype;
export using ::napi_bigint;
export using ::napi_boolean;
export using ::napi_external;
export using ::napi_function;
export using ::napi_null;
export using ::napi_number;
export using ::napi_object;
export using ::napi_string;
export using ::napi_symbol;
export using ::napi_undefined;

// napi_typedarray_type
export using ::napi_typedarray_type;
export using ::napi_bigint64_array;
export using ::napi_biguint64_array;
export using ::napi_float32_array;
export using ::napi_float64_array;
export using ::napi_int16_array;
export using ::napi_int32_array;
export using ::napi_int8_array;
export using ::napi_typedarray_type;
export using ::napi_uint16_array;
export using ::napi_uint32_array;
export using ::napi_uint8_array;
export using ::napi_uint8_clamped_array;

// napi_status
export using ::napi_status;
export using ::napi_array_expected;
export using ::napi_arraybuffer_expected;
export using ::napi_bigint_expected;
export using ::napi_boolean_expected;
export using ::napi_callback_scope_mismatch;
export using ::napi_cancelled;
export using ::napi_cannot_run_js;
export using ::napi_closing;
export using ::napi_date_expected;
export using ::napi_detachable_arraybuffer_expected;
export using ::napi_escape_called_twice;
export using ::napi_function_expected;
export using ::napi_generic_failure;
export using ::napi_handle_scope_mismatch;
export using ::napi_invalid_arg;
export using ::napi_name_expected;
export using ::napi_no_external_buffers_allowed;
export using ::napi_number_expected;
export using ::napi_object_expected;
export using ::napi_ok;
export using ::napi_pending_exception;
export using ::napi_queue_full;
export using ::napi_string_expected;
export using ::napi_would_deadlock;

// napi_property_attributes
export using ::napi_property_attributes;
export using ::napi_default;
export using ::napi_writable;
export using ::napi_enumerable;
export using ::napi_configurable;
export using ::napi_static;

// NOLINTEND(misc-unused-using-decls)
