add_library(ivm_backend_v8 SHARED)
target_sources(ivm_backend_v8
	PRIVATE
		agent.cc
		main.cc
		module.cc
		realm.cc
		runtime/runtime.cc
		script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		agent.h.cc
		environment.cc
		lock.cc
		module.h.cc
		realm.h.cc
		runtime/runtime.h.cc
		script.h.cc
		utility.cc
)
target_embed_content(ivm_backend_v8 runtime/dist/interface.js)
target_include_directories(ivm_backend_v8 SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
target_link_libraries(ivm_backend_v8 PUBLIC
	utility
	napi_js
	nodejs
	value_js
	v8_js
)

# Windows linking
if(WIN32)
	string(TOLOWER CMAKE_HOST_SYSTEM_PROCESSOR NODE_ARCH)
	target_link_directories(ivm_backend_v8 PUBLIC ${NODE_DIST_DIR}/windows/${NODE_ARCH})
	target_link_libraries(ivm_backend_v8 PUBLIC node dbghelp winmm)
endif()

# Copy out to ./dist
execute_process(
	COMMAND sh -C "${CMAKE_SOURCE_DIR}/scripts/cmake/host_platform"
	OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE HOST_PLATFORM
	RESULT_VARIABLE RESULT)
if(NOT RESULT STREQUAL 0)
	message(FATAL_ERROR "Unknown host platform." )
endif()
add_custom_command(TARGET ivm_backend_v8 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		$<TARGET_FILE:ivm_backend_v8>
		${CMAKE_SOURCE_DIR}/packages/isolated-vm/dist/backend_v8/${HOST_PLATFORM}/backend_v8.node
)
