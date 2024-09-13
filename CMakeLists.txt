cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)

# Variables
if (NOT DEFINED IVM_HOST_PLATFORM)
	message(FATAL_ERROR "IVM_HOST_PLATFORM is not set")
endif()
if (NOT DEFINED IVM_HOST_ARCH)
	message(FATAL_ERROR "IVM_HOST_ARCH is not set")
elseif(IVM_HOST_ARCH STREQUAL "aarch64")
	set(IVM_HOST_ARCH "arm64")
endif()

if (NOT DEFINED V8_INCLUDE_DIR)
	message(FATAL_ERROR "V8_INCLUDE_DIR is not set")
endif()
if (NOT DEFINED V8_INCLUDE_GN_FILE)
	message(FATAL_ERROR "V8_INCLUDE_GN_FILE is not set")
endif()
if (NOT DEFINED V8_LIBRARY_TYPE)
	message(FATAL_ERROR "V8_LIBRARY_TYPE is not set")
endif()
if (NOT DEFINED V8_OUT_PATH)
	message(FATAL_ERROR "V8_OUT_PATH is not set")
endif()

# Project config
project(isolated_vm LANGUAGES CXX)

# Boost requirement
find_package(Boost 1.83)

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
	# `std::stop_token` and maybe others
	add_compile_definitions(_LIBCPP_ENABLE_EXPERIMENTAL)
	# Required for clangd w/ modules
	add_compile_options(-fretain-comments-from-system-headers)
	# This enables `#include <expected>` somehow
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20)
		add_compile_options(-D__cpp_concepts=202002L -Wno-builtin-macro-redefined)
	endif()
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
	# clang-18 complains about `std::aligned_storage` within `std::shared_ptr` compiling against
	# libstdc++
	add_compile_options(
		-Wno-deprecated-declarations
	)
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
		packages/value/date.cc
		packages/value/dictionary.cc
		packages/value/discriminated_union.cc
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
	PRIVATE
		packages/common-v8/value/array.cc
		packages/common-v8/value/object.cc
		packages/common-v8/value/string.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/common-v8/accept.cc
		packages/common-v8/utility.cc
		packages/common-v8/include/_module.cc
		packages/common-v8/include/array.cc
		packages/common-v8/include/boolean.cc
		packages/common-v8/include/date.cc
		packages/common-v8/include/external.cc
		packages/common-v8/include/handle.cc
		packages/common-v8/include/number.cc
		packages/common-v8/include/object.cc
		packages/common-v8/include/string.cc
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
target_compile_options(ivm_v8_embedded PRIVATE -include ${V8_INCLUDE_GN_FILE})
target_include_directories(ivm_v8_embedded SYSTEM PRIVATE ${V8_INCLUDE_DIR})
target_link_libraries(ivm_v8_embedded PUBLIC ivm_base ivm_utility)
if (V8_LIBRARY_TYPE STREQUAL "static")
	link_directories(${V8_OUT_PATH}/obj)
	target_link_libraries(ivm_v8_embedded PUBLIC v8_monolith)
else()
	link_directories(${V8_OUT_PATH})
	target_link_libraries(ivm_v8_embedded PUBLIC v8 v8_libbase v8_libplatform)
endif()

# Linked nodejs v8 interface
add_library(ivm_v8_nodejs EXCLUDE_FROM_ALL OBJECT)
target_compile_definitions(ivm_v8_nodejs PRIVATE IVM_V8_VIA_NODEJS)
target_sources(ivm_v8_nodejs ${IVM_V8_COMMON_SOURCES})
target_include_directories(ivm_v8_nodejs SYSTEM PRIVATE ${NODE_INCLUDE_DIR})
target_link_libraries(ivm_v8_nodejs PUBLIC ivm_base ivm_utility)

# isolated v8 backend
add_library(ivm_isolated_v8 EXCLUDE_FROM_ALL STATIC)
target_compile_definitions(ivm_isolated_v8 PRIVATE NAPI_EXPERIMENTAL)
target_sources(ivm_isolated_v8
	PRIVATE
		packages/libivm-v8/agent/clock.cc
		packages/libivm-v8/agent/host.cc
		packages/libivm-v8/agent/task_runner.cc
		packages/libivm-v8/cluster.cc
		packages/libivm-v8/platform/handle.cc
		packages/libivm-v8/platform/job_handle.cc
		packages/libivm-v8/platform/platform.cc
		packages/libivm-v8/realm.cc
		packages/libivm-v8/scheduler.cc
		packages/libivm-v8/script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/libivm-v8/include/_module.cc
		packages/libivm-v8/include/agent/agent.cc
		packages/libivm-v8/include/agent/clock.cc
		packages/libivm-v8/include/agent/host.cc
		packages/libivm-v8/include/agent/task_runner.cc
		packages/libivm-v8/include/cluster.cc
		packages/libivm-v8/include/platform.cc
		packages/libivm-v8/include/platform/task_runner.cc
		packages/libivm-v8/include/realm.cc
		packages/libivm-v8/include/scheduler.cc
		packages/libivm-v8/include/script.cc
)
target_compile_definitions(ivm_isolated_v8 PRIVATE ${IVM_TRICKSHOT_DEFINES})
target_link_libraries(ivm_isolated_v8 PUBLIC ivm_base ivm_utility ivm_v8_embedded)

# nodejs require-able module
add_library(ivm_backend_v8 EXCLUDE_FROM_ALL SHARED)
target_sources(ivm_backend_v8
	PRIVATE
		packages/nodejs/agent.cc
		packages/nodejs/environment.cc
		packages/nodejs/main.cc
		packages/nodejs/script.cc
		packages/nodejs/value/arguments.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		packages/nodejs/include/_module.cc
		packages/nodejs/include/arguments.cc
		packages/nodejs/include/environment.cc
		packages/nodejs/include/external.cc
		packages/nodejs/utility.cc
		packages/nodejs/value/visit.cc
		packages/third_party/napi.cc
)
target_compile_definitions(ivm_backend_v8 PRIVATE
	NODE_ADDON_API_DISABLE_DEPRECATED
)
target_include_directories(ivm_backend_v8 SYSTEM PRIVATE ${NAPI_INCLUDE_DIR} ${NODE_INCLUDE_DIR})
target_link_libraries(ivm_backend_v8 PUBLIC ivm_base ivm_isolated_v8 ivm_utility ivm_v8_nodejs)

# Copy out to ./dist
add_custom_command(TARGET ivm_backend_v8 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_SONAME_FILE:ivm_backend_v8> ${CMAKE_CURRENT_SOURCE_DIR}/dist/backend_v8/${IVM_HOST_PLATFORM}-${IVM_HOST_ARCH}/backend_v8.node
)
