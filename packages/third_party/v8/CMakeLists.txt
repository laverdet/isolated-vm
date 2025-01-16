add_library(${IV8_PREFIX}_v8 STATIC)
target_sources(${IV8_PREFIX}_v8
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
target_include_directories(${IV8_PREFIX}_v8 SYSTEM PRIVATE ${IV8_INCLUDE_DIR})

# Preprocessor macros which enable dual v8 in the same process
if (IVM_IV8_TRICKSHOT)
	target_compile_definitions(${IV8_PREFIX}_v8 PUBLIC
		cppgc=zppgc
		iv8=iz8
		unibrow=znibrow
		v8_crdtp=z8_crdtp
		V8_Dcheck=Z8_Dcheck
		V8_Fatal=Z8_Fatal
		v8_inspector=z8_inspector
		v8=z8
	)
endif()

if (IVM_IV8_EMBEDDED)
	# Include v8-gn.h
	target_compile_options(${IV8_PREFIX}_v8 PRIVATE -include ${V8_INCLUDE_GN_FILE})

	# Link against v8, which is built elsewhere
	if (V8_LIBRARY_TYPE STREQUAL "static")
		target_link_directories(${IV8_PREFIX}_v8 PUBLIC ${V8_OUT_PATH}/obj)
		target_link_libraries(${IV8_PREFIX}_v8 PUBLIC v8_monolith)
	else()
		target_link_directories(${IV8_PREFIX}_v8 PUBLIC ${V8_OUT_PATH})
		target_link_libraries(${IV8_PREFIX}_v8 PUBLIC v8 v8_libbase v8_libplatform)
	endif()
else()
	# Prevents deeper v8 headers from being included from nodejs
	target_compile_definitions(${IV8_PREFIX}_v8 PRIVATE IVM_V8_VIA_NODEJS)
endif()
