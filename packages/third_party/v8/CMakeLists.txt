add_library(google_v8 STATIC)

# Preprocessor macros which enable dual v8 in the same process
if (IVM_V8_TRICKSHOT)
	set(MODULE_NAME z8)
	target_compile_definitions(google_v8 PUBLIC
		cppgc=zppgc
		unibrow=znibrow
		v8_crdtp=z8_crdtp
		V8_Dcheck=Z8_Dcheck
		V8_Fatal=Z8_Fatal
		v8_inspector=z8_inspector
		v8=z8
	)
	target_include_directories(google_v8 SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
else()
	set(MODULE_NAME v8)
endif()

# Sources which will be preprocessed by `expand_def`
target_preprocessed_sources(google_v8 v8 ${MODULE_NAME}
	PUBLIC FILE_SET CXX_MODULES FILES
		array_buffer.cc
		container.cc
		context.cc
		data.cc
		date.cc
		external.cc
		function_callback.cc
		function.cc
		initialization.cc
		isolate.cc
		libplatform.cc
		local_handle.cc
		locker.cc
		maybe.cc
		memory_span.cc
		message.cc
		object.cc
		persistent_handle.cc
		platform.cc
		primitive.cc
		promise.cc
		script.cc
		template.cc
		v8.cc
		value.cc
		weak_callback_info.cc
)

# v8 headers
target_include_directories(google_v8 SYSTEM PRIVATE "${V8_INCLUDE_DIR}")

if (IVM_V8_EMBEDDED)
	# Include v8-gn.h
	target_compile_options(google_v8 PRIVATE -include "${V8_INCLUDE_GN_FILE}")

	# Link against v8, which is built elsewhere
	if (V8_LIBRARY_TYPE STREQUAL static)
		target_link_directories(google_v8 PUBLIC "${V8_OUT_PATH}/obj")
		target_link_libraries(google_v8 PUBLIC v8_monolith)
	else()
		target_link_directories(google_v8 PUBLIC "${V8_OUT_PATH}")
		target_link_libraries(google_v8 PUBLIC v8 v8_libbase v8_libplatform)
	endif()
else()
	# Prevents deeper v8 headers from being included from nodejs
	target_compile_definitions(google_v8 PRIVATE V8_VIA_NODEJS)
endif()
