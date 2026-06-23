# Locate dependencies (napi_exports is always `nodejs`)
find_package_from_npm(@auto_js/js AS @auto_js/${AUTO_JS_PREFIX}js PROPAGATE auto_js)
find_package_from_npm(@auto_js/napi_exports)
find_package_from_npm(@auto_js/v8_exports PROPAGATE google_v8)

# Initialize target [napi_js or napi_isolated_js]
set(napi_js ${AUTO_JS_PREFIX}napi_js)
add_library(${napi_js} STATIC)
set_npm_package_properties(@auto_js/napi auto_js napi_js)
target_compile_features(${napi_js} PUBLIC cxx_std_23)
target_include_directories(${napi_js} INTERFACE include)
target_link_libraries(${napi_js} PRIVATE ${auto_js} ${google_v8} utility_js)
target_link_libraries(${napi_js} PUBLIC nodejs)

target_sources(${napi_js}
	PRIVATE
		api/napi_scheduler.cc
		api/uv_dlib.cc
		api/uv_scheduler.cc
		support/environment.cc
		support/host.cc
		support/initialize.cc
		transfer/accept.cc
		value/array_buffer.cc
		value/array.cc
		value/object.cc
		value/primitive.cc
		value/record.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		api/api.cc
		api/callback_info.cc
		api/finalizer.cc
		api/handle_scope.cc
		api/invoke.cc
		api/napi_scheduler.h.cc
		api/threadsafe_function.cc
		api/uv_dlib.h.cc
		api/uv_handle.cc
		api/uv_scheduler.h.cc
		handle/bound_value.cc
		handle/reference.cc
		handle/remote.cc
		handle/types.cc
		handle/value.cc
		support/callback_storage.cc
		support/callback.cc
		support/class_template_storage.cc
		support/environment_fwd.cc
		support/environment.h.cc
		support/error_scope.cc
		support/host.h.cc
		support/initialize.h.cc
		support/promise.cc
		support/string_table.cc
		support/utility.cc
		transfer/accept.h.cc
		transfer/visit.cc
		value/array_buffer.h.cc
		value/array.h.cc
		value/class.cc
		value/class.h.cc
		value/external.cc
		value/function.cc
		value/function.h.cc
		value/object.h.cc
		value/primitive.h.cc
		value/record.h.cc
		value/value.cc
)

# Enable "delay load" for win32 otherwise the dll will attempt to load node.exe from PATH as a
# shared library and then start invoking functions from it.
if(WIN32)
	set(napi_js_delay_load ${napi_js}_delay_load)
	add_library(${napi_js_delay_load} STATIC)
	set_target_properties(${napi_js_delay_load} PROPERTIES CXX_VISIBILITY_PRESET default)
	target_sources(${napi_js_delay_load} PUBLIC support/win32_delay_load_hook.cc)
	target_link_libraries(${napi_js_delay_load} PUBLIC delayimp.lib)
	target_link_libraries(${napi_js} PUBLIC ${napi_js_delay_load})
	target_link_options(${napi_js_delay_load} PUBLIC "-Wl,/DELAYLOAD:node.exe")
endif()
