module;
#include <node_api_types.h>
export module nodejs:node_api_types;
// NOLINTBEGIN(misc-unused-using-decls)

export using ::napi_async_cleanup_hook;
export using ::napi_cleanup_hook;
export using ::napi_threadsafe_function;

// napi_threadsafe_function_release_mode
export using ::napi_threadsafe_function_release_mode;
export using ::napi_tsfn_abort;
export using ::napi_tsfn_release;

// napi_threadsafe_function_call_mode
export using ::napi_threadsafe_function_call_mode;
export using ::napi_tsfn_blocking;
export using ::napi_tsfn_nonblocking;

// NOLINTEND(misc-unused-using-decls)
