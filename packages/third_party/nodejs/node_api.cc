module;
#include <node_api.h>
export module nodejs:node_api;
// NOLINTBEGIN(misc-unused-using-decls)

export using ::napi_add_async_cleanup_hook;
export using ::napi_add_env_cleanup_hook;
export using ::napi_async_cleanup_hook_handle;
export using ::napi_get_uv_event_loop;
export using ::napi_remove_async_cleanup_hook;

// NOLINTEND(misc-unused-using-decls)
