module;
#include "napi_js/extern.h"
#include <node_api.h>
export module nodejs:node_api;
// NOLINTBEGIN(misc-unused-using-decls)

// functions
export using ::napi_acquire_threadsafe_function;
export using ::napi_add_async_cleanup_hook;
export using ::napi_add_env_cleanup_hook;
export using ::napi_add_env_cleanup_hook;
export using ::napi_async_cleanup_hook_handle;
export using ::napi_call_threadsafe_function;
export using ::napi_create_threadsafe_function;
export using ::napi_get_threadsafe_function_context;
export using ::napi_get_uv_event_loop;
export using ::napi_ref_threadsafe_function;
export using ::napi_release_threadsafe_function;
export using ::napi_remove_async_cleanup_hook;
export using ::napi_threadsafe_function_call_js;
export using ::napi_unref_threadsafe_function;

// NOLINTEND(misc-unused-using-decls)
