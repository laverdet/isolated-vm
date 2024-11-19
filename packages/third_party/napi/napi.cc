module;
#include <js_native_api.h>
#include <napi.h>
export module napi;
export using ::napi_callback_info;
export using ::napi_env;
export using ::napi_status;
export using ::napi_value;
export using ::napi_valuetype;

export using ::napi_create_array_with_length;
export using ::napi_create_array;
export using ::napi_create_bigint_int64;
export using ::napi_create_bigint_uint64;
export using ::napi_create_date;
export using ::napi_create_double;
export using ::napi_create_int32;
export using ::napi_create_int64;
export using ::napi_create_object;
export using ::napi_create_string_latin1;
export using ::napi_create_string_utf16;
export using ::napi_create_uint32;
export using ::napi_get_array_length;
export using ::napi_get_boolean;
export using ::napi_get_cb_info;
export using ::napi_get_element;
export using ::napi_get_instance_data;
export using ::napi_get_null;
export using ::napi_get_property_names;
export using ::napi_get_property;
export using ::napi_get_undefined;
export using ::napi_has_own_property;
export using ::napi_is_array;
export using ::napi_is_date;
export using ::napi_is_promise;
export using ::napi_set_element;
export using ::napi_set_property;
export using ::napi_typeof;

namespace Napi {
export using Napi::CallbackInfo;
export using Napi::Env;
export using Napi::Function;
export using Napi::Promise;
export using Napi::TypedThreadSafeFunction;
} // namespace Napi
