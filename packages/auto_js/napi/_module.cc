export module napi_js;
// nb: These must come first, for some reason
import :accept;
import :visit;
// Before these:
export import :api; // should be removed
export import :api.uv_dlib;
export import :class_template_storage;
export import :environment;
export import :external;
export import :function;
export import :initialize;
export import :object;
export import :promise;
export import :reference;
export import :remote;
export import :string_table;
export import :value;
import :class_definitions;
import :function_definitions;
#if __GNUC__ > 14
export import :accept;
export import :array_buffer;
export import :array;
export import :bound_value;
export import :callback;
export import :class_;
export import :class_definitions;
export import :container;
export import :error_scope;
export import :function_definitions;
export import :handle.types;
export import :primitive;
export import :record;
export import :utility;
export import :value_handle;
export import :visit;
#endif
