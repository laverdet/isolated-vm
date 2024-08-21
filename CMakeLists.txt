cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)

# Variables
set(ARCH "arm64")
set(PLATFORM "linux")
set(V8_INCLUDE_DIR "$ENV{V8_INCLUDE}")
set(V8_LIBRARY_TYPE "$ENV{V8_LIBRARY}")
set(V8_TARGET_DIR "$ENV{V8_TARGET}")

# Project config
project(isolated_vm LANGUAGES CXX)

# Boost requirement
find_package(Boost 1.83 CONFIG COMPONENTS thread)

# C++26 environment w/ `import std`
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler flags
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options(
	-fdiagnostics-color
	-Wall
	-Wextra
	-Wpedantic
)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# Required for clangd w/ modules
	add_compile_options(-fretain-comments-from-system-headers)
endif()

# Release type compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_options(
		-fsanitize=address
		-Wno-unused-but-set-variable
		-Wno-unused-result
	)
	add_link_options(
		-fsanitize=address
		-shared-libasan
	)
else()
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()

# Generic abstract C++ utilities
add_library(ivm_utility EXCLUDE_FROM_ALL OBJECT)
target_sources(ivm_utility
	PUBLIC
		FILE_SET CXX_MODULES FILES
			packages/utility/_module.cc
			packages/utility/byte_literals.cc
			packages/utility/call_once_or_else.cc
			packages/utility/collection_group.cc
			packages/utility/comparator.cc
			packages/utility/lockable.cc
			packages/utility/mutable_value.cc
			packages/utility/prehashed_string_map.cc
			packages/utility/type_traits.cc
			packages/utility/utility.cc
)

# JavaScript adjacent code which is not specific to v8 or nodejs
add_library(ivm_base EXCLUDE_FROM_ALL OBJECT)
target_sources(ivm_base
	PRIVATE
		packages/value/assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/value/_module.cc
		packages/value/accept.cc
		packages/value/dictionary.cc
		packages/value/enum.cc
		packages/value/object.cc
		packages/value/primitive.cc
		packages/value/tag.cc
		packages/value/transfer.cc
		packages/value/variant.cc
		packages/value/visit.cc
)
target_link_libraries(ivm_base PRIVATE ivm_utility)

# v8 common packages. These source files are built twice, once which links against nodejs, and
# another which links against internal v8/z8.
set(IVM_V8_COMMON_SOURCES
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/common-v8/_module.cc
		packages/common-v8/accept.cc
		packages/common-v8/utility.cc
		packages/common-v8/value/array.cc
		packages/common-v8/value/boolean.cc
		packages/common-v8/value/date.cc
		packages/common-v8/value/external.cc
		packages/common-v8/value/handle.cc
		packages/common-v8/value/number.cc
		packages/common-v8/value/object.cc
		packages/common-v8/value/string.cc
		packages/common-v8/visit.cc
		packages/third_party/v8/array_buffer.cc
		packages/third_party/v8/container.cc
		packages/third_party/v8/context.cc
		packages/third_party/v8/data.cc
		packages/third_party/v8/date.cc
		packages/third_party/v8/external.cc
		packages/third_party/v8/initialization.cc
		packages/third_party/v8/isolate.cc
		packages/third_party/v8/libplatform.cc
		packages/third_party/v8/local_handle.cc
		packages/third_party/v8/locker.cc
		packages/third_party/v8/maybe.cc
		packages/third_party/v8/object.cc
		packages/third_party/v8/persistent_handle.cc
		packages/third_party/v8/platform.cc
		packages/third_party/v8/primitive.cc
		packages/third_party/v8/script.cc
		packages/third_party/v8/v8.cc
		packages/third_party/v8/value.cc
		packages/third_party/v8/weak_callback_info.cc
)
set(IVM_TRICKSHOT_DEFINES
	iv8=iz8
	v8_crdtp=z8_crdtp
	V8_Dcheck=Z8_Dcheck
	V8_Fatal=Z8_Fatal
	v8_inspector=z8_inspector
	v8=z8
)

# Embedded v8 interface
# Note: `target_link_libraries` of OBJECT isn't inherited. So ivm_base and ivm_utility must be
# linked to the final target.
# https://gitlab.kitware.com/cmake/cmake/-/issues/18090
add_library(ivm_v8_embedded EXCLUDE_FROM_ALL OBJECT)
target_sources(ivm_v8_embedded ${IVM_V8_COMMON_SOURCES})
target_compile_definitions(ivm_v8_embedded PRIVATE ${IVM_TRICKSHOT_DEFINES})
target_compile_options(ivm_v8_embedded PRIVATE -include ${V8_TARGET_DIR}/include/v8-gn.h)
target_include_directories(ivm_v8_embedded SYSTEM PRIVATE ${V8_INCLUDE_DIR})
target_link_libraries(ivm_v8_embedded PUBLIC ivm_base ivm_utility)
if (V8_LIBRARY_TYPE STREQUAL "static")
	link_directories(${V8_TARGET_DIR}/obj)
	target_link_libraries(ivm_v8_embedded PUBLIC v8_monolith)
else()
	link_directories(${V8_TARGET_DIR})
	target_link_libraries(ivm_v8_embedded PUBLIC v8 v8_libbase v8_libplatform)
endif()

# Linked nodejs v8 interface
add_library(ivm_v8_nodejs EXCLUDE_FROM_ALL OBJECT)
target_sources(ivm_v8_nodejs ${IVM_V8_COMMON_SOURCES})
target_include_directories(ivm_v8_nodejs SYSTEM PRIVATE /opt/include/node)
target_link_libraries(ivm_v8_nodejs PUBLIC ivm_base ivm_utility)

# isolated v8 backend
add_library(ivm_isolated_v8 EXCLUDE_FROM_ALL STATIC)
target_sources(ivm_isolated_v8
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/libivm-v8/_module.cc
		packages/libivm-v8/agent/agent.cc
		packages/libivm-v8/agent/host.cc
		packages/libivm-v8/agent/lock.cc
		packages/libivm-v8/agent/storage.cc
		packages/libivm-v8/cluster.cc
		packages/libivm-v8/platform.cc
		packages/libivm-v8/platform_handle.cc
		packages/libivm-v8/realm.cc
		packages/libivm-v8/scheduler.cc
		packages/libivm-v8/script.cc
)
target_compile_definitions(ivm_isolated_v8 PRIVATE ${IVM_TRICKSHOT_DEFINES})
target_link_libraries(ivm_isolated_v8 PUBLIC ivm_base ivm_utility ivm_v8_embedded)

# nodejs require-able module
add_library(ivm_backend_v8 EXCLUDE_FROM_ALL SHARED)
target_sources(ivm_backend_v8
	PUBLIC
		packages/nodejs/main.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/nodejs/_module.cc
		packages/nodejs/agent.cc
		packages/nodejs/arguments.cc
		packages/nodejs/environment.cc
		packages/nodejs/make_promise.cc
		packages/nodejs/script.cc
		packages/nodejs/visit.cc
		packages/third_party/napi.cc
)
target_compile_definitions(ivm_backend_v8 PRIVATE
	NODE_ADDON_API_DISABLE_DEPRECATED
)
target_include_directories(ivm_backend_v8 SYSTEM PRIVATE /opt/include/node ${NAPI_INCLUDE_DIR})
target_link_libraries(ivm_backend_v8 PUBLIC ivm_base ivm_isolated_v8 ivm_utility ivm_v8_nodejs)

# Copy out to ./dist
add_custom_command(TARGET ivm_backend_v8 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_SONAME_FILE:ivm_backend_v8> ${CMAKE_CURRENT_SOURCE_DIR}/dist/libivm.backend_v8.node
)

# Development testing of the library
add_executable(ivm_test EXCLUDE_FROM_ALL)
target_sources(ivm_test PUBLIC test.cc)
target_link_libraries(ivm_test PUBLIC ivm_base ivm_utility)
set_target_properties(ivm_test PROPERTIES LINK_FLAGS "-lstdc++")
