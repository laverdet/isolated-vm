# Configure and locate dependencies
set(NAPI_VERSION 10)
set(V8_TRICKSHOT ON)
find_package_from_npm(@auto_js/js)
find_package_from_npm(@auto_js/napi)
find_package_from_npm(@auto_js/v8 @isolated-vm/embedded_v8_js)

# Create a build target: 'backend_napi_v8.triplet' which compiles, but does not "install". This is
# so that the binary in `./build/` matches the distributed binary's filename.
triplet_util(HOST_PLATFORM %p)
set(BACKEND_NAME "backend_napi_v8.${HOST_PLATFORM}")
if(COMPILE_ONLY)
	add_library("${BACKEND_NAME}" OBJECT)
else()
	add_library("${BACKEND_NAME}" SHARED)
endif()

# Backend sources
target_sources("${BACKEND_NAME}"
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
target_embed_content("${BACKEND_NAME}" runtime/dist/interface.js runtime_dist_interface_js)

# Detect musl libc for dlopen workaround
triplet_util(HOST_TOOLCHAIN %e)
if(HOST_TOOLCHAIN STREQUAL -musl)
	target_compile_definitions("${BACKEND_NAME}" PRIVATE __MUSL__)
endif()

# Link backend target
target_link_libraries("${BACKEND_NAME}" PUBLIC auto_js embedded_v8_js isolated_vm napi_js nodejs utility_js)

# Emit `xx.node` instead of `libxx.so`
set_target_properties("${BACKEND_NAME}" PROPERTIES PREFIX "" SUFFIX ".node")

# Windows linking
if(WIN32)
	string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} NODE_ARCH)
	if(NODE_ARCH STREQUAL amd64)
		set(NODE_ARCH x64)
	endif()
	target_link_directories("${BACKEND_NAME}" PUBLIC ${NODE_DIST_DIR}/win-${NODE_ARCH})
endif()

# Addon 'isolated_vm' module is built with additional sources. This target is initialized in
# `packages/isolated-vm/addon`
target_sources(isolated_vm
	PRIVATE
		api/value/array_buffer.cc
		api/value/array.cc
		api/value/function.cc
		api/value/object.cc
		api/value/primitive.cc
		api/value/prototype.cc
		api/value/record.cc
		api/value/vector.cc
	PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/api" FILES
		api/support/callback_info.cc
		api/support/callback.cc
		api/support/cast.cc
		api/support/initialize.cc
		api/support/lock.cc
)
target_link_libraries(isolated_vm PRIVATE embedded_v8_js)

# An additional target 'backend_napi_v8' which copies or links the binary where nodejs can find it.
add_custom_target(backend_napi_v8)
add_dependencies(backend_napi_v8 "${BACKEND_NAME}")

# Install
if(NOT DEFINED IVM_INSTALL)
	set(IVM_INSTALL link)
endif()
if(IVM_INSTALL STREQUAL artifacts)
	# Copy to `artifacts`; `napi-rs` will regenerate `package.json` later. We still need the
	# placeholder `package.json` for testing.
	set(LOCAL_BACKEND_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/artifacts/${HOST_PLATFORM}")
	add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
		"${CMAKE_CURRENT_BINARY_DIR}/backend_napi_v8.${HOST_PLATFORM}.node"
		"${LOCAL_BACKEND_PACKAGE}/")
	if(WIN32)
		add_custom_command(
			TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_CURRENT_BINARY_DIR}/backend_napi_v8.${HOST_PLATFORM}.lib"
			"${LOCAL_BACKEND_PACKAGE}/")
	endif()
elseif(IVM_INSTALL STREQUAL link)
	# Setup development package
	set(LOCAL_BACKEND_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}")
	add_custom_command(
		TARGET backend_napi_v8 POST_BUILD COMMAND ${CMAKE_COMMAND} -E create_symlink
		"${CMAKE_CURRENT_BINARY_DIR}"
		"${CMAKE_SOURCE_DIR}/packages/isolated-vm/node_modules/@isolated-vm/experimental-${HOST_PLATFORM}")
endif()
file(WRITE "${LOCAL_BACKEND_PACKAGE}/package.json" "{\"main\":\"backend_napi_v8.${HOST_PLATFORM}.node\"}\n")
