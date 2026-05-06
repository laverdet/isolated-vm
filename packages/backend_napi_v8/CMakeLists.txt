set(NAPI_VERSION 10)
set(V8_TRICKSHOT ON)
find_package_from_npm(@auto_js/js)
find_package_from_npm(@auto_js/napi)
find_package_from_npm(@auto_js/v8 @isolated-vm/embedded_v8_js)

if(COMPILE_ONLY)
	add_library(backend_napi_v8 OBJECT)
else()
	add_library(backend_napi_v8 SHARED)
endif()

# Sources
target_sources(backend_napi_v8
	PRIVATE
		agent.cc
		environment.cc
		main.cc
		module.cc
		native_module.cc
		realm.cc
		reference.cc
		runtime/runtime.cc
		script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		agent.h.cc
		environment.h.cc
		lock.cc
		module.h.cc
		native_module.h.cc
		realm.h.cc
		reference.h.cc
		runtime/runtime.h.cc
		script.h.cc
		utility.cc
)

target_embed_content(backend_napi_v8 runtime/dist/interface.js runtime_dist_interface_js)
target_link_libraries(backend_napi_v8 PUBLIC auto_js embedded_v8_js isolated_vm napi_js nodejs utility_js)

# Addon 'isolated_vm' module is built with additional sources
target_link_libraries(isolated_vm PRIVATE embedded_v8_js)
target_sources(isolated_vm
	PRIVATE
		api/value/accept.cc
		api/value/primitive.cc
		api/value/value.cc
	PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/api" FILES
		api/support/callback_info.cc
		api/support/callback.cc
		api/support/cast.cc
		api/support/initialize.cc
		api/support/lock.cc
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

# Emit `xx.node` instead of `libxx.so`
set_target_properties(backend_napi_v8 PROPERTIES PREFIX "" SUFFIX ".node")

# `triplet_util` for host platform [darwin, win32, linux]
execute_process(
	COMMAND sh -c "${CMAKE_SOURCE_DIR}/scripts/triplet_util %p"
	OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE HOST_PLATFORM
	RESULT_VARIABLE RESULT)
if(NOT RESULT STREQUAL 0)
	message(FATAL_ERROR "Unknown host platform." )
endif()

# Install
if(IVM_INSTALL STREQUAL link)
	# Setup development package
	set(LOCAL_BACKEND_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}")
	add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink
		"${CMAKE_CURRENT_BINARY_DIR}"
		"${CMAKE_SOURCE_DIR}/packages/isolated-vm/node_modules/@isolated-vm/experimental-${HOST_PLATFORM}")
	file(WRITE "${LOCAL_BACKEND_PACKAGE}/package.json" "{\"main\":\"backend_napi_v8.node\"}\n")
elseif(IVM_INSTALL STREQUAL copy)
	# Copy to `artifacts`. `napi-rs` will generate `package.json` later
	set(LOCAL_BACKEND_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${HOST_PLATFORM}")
	add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
		"$<TARGET_FILE:backend_napi_v8>"
		"${LOCAL_BACKEND_PACKAGE}/backend_napi_v8.${HOST_PLATFORM}.node")
	file(WRITE "${LOCAL_BACKEND_PACKAGE}/package.json" "{\"main\":\"backend_napi_v8.${HOST_PLATFORM}.node\"}\n")
endif()
