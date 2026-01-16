add_library(backend_napi_v8 SHARED)
target_sources(backend_napi_v8
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
target_embed_content(backend_napi_v8 runtime/dist/interface.js)
target_include_directories(backend_napi_v8 SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
target_link_libraries(backend_napi_v8 PUBLIC
	napi_js
	nodejs
	utility
	v8_js
	value_js
)

# Windows linking
if(WIN32)
	string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} NODE_ARCH)
	if(NODE_ARCH STREQUAL amd64)
		set(NODE_ARCH x64)
	endif()
	target_link_directories(backend_napi_v8 PUBLIC ${NODE_DIST_DIR}/windows/${NODE_ARCH})
	target_link_libraries(backend_napi_v8 PUBLIC node dbghelp winmm)
endif()

# Copy to `.node` for require (must be copy, not symlink, otherwise node will try to load it as a script)
execute_process(
	COMMAND sh -c "${CMAKE_SOURCE_DIR}/scripts/triplet_util %p"
	OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE HOST_PLATFORM
	RESULT_VARIABLE RESULT)
if(NOT RESULT STREQUAL 0)
	message(FATAL_ERROR "Unknown host platform." )
endif()
set(LOCAL_BACKEND_PACKAGE ${CMAKE_CURRENT_BINARY_DIR}/artifacts/${HOST_PLATFORM})
add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
		$<TARGET_FILE:backend_napi_v8>
		${LOCAL_BACKEND_PACKAGE}/backend_napi_v8.${HOST_PLATFORM}.node)
file(WRITE ${LOCAL_BACKEND_PACKAGE}/package.json "{\"main\":\"backend_napi_v8.${HOST_PLATFORM}.node\"}\n")

# Link to `node_modules` for development
set(HOST_BACKEND_PACKAGE ${CMAKE_SOURCE_DIR}/packages/isolated-vm/node_modules/@isolated-vm/experimental-${HOST_PLATFORM})
if(IVM_INSTALL STREQUAL link)
	add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink
		${LOCAL_BACKEND_PACKAGE}
		${HOST_BACKEND_PACKAGE})
endif()
