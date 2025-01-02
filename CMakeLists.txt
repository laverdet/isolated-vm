cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)

# Variables
if (NOT DEFINED IVM_HOST_PLATFORM)
	message(FATAL_ERROR "IVM_HOST_PLATFORM is not set")
endif()
if (NOT DEFINED IVM_HOST_ARCH)
	message(FATAL_ERROR "IVM_HOST_ARCH is not set")
elseif(IVM_HOST_ARCH STREQUAL aarch64)
	set(IVM_HOST_ARCH arm64)
endif()

if (NOT DEFINED NODE_DIST_DIR)
	message(FATAL_ERROR "NODE_DIST_DIR is not set")
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

# Ensure absolute path for variables used in subdirectories
cmake_path(ABSOLUTE_PATH NODE_DIST_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_INCLUDE_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_INCLUDE_GN_FILE BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_OUT_PATH BASE_DIRECTORY ${CMAKE_SOURCE_DIR})

# Project config
project(isolated_vm LANGUAGES CXX)

# Boost requirement
find_package(Boost 1.83 REQUIRED)

# C++26 environment w/ `import std`
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler flags
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# No warnings allowed in release
if (CMAKE_BUILD_TYPE STREQUAL Release)
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()

# Apple dylib flags
if (APPLE)
	set(CMAKE_SHARED_LINKER_FLAGS "-dynamiclib -undefined dynamic_lookup")
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12)
endif()

# Microsoft Visual C++. It can't actually compile the templates in `ivm::js`, but this would be
# required if it could.
if (MSVC)
	# Lmfao: "After 20+ years, we’re finally able to set the value for “__cplusplus” macro forward in
	# the MSVC compiler."
	# https://developercommunity.visualstudio.com/t/msvc-incorrectly-defines-cplusplus/139261#T-N223868
	# https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
	add_compile_options(/Zc:__cplusplus)
endif()

# Windows flags. You want /MT and not /MD
if (WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

# Windows Clang flags
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	# error: unknown attribute 'no_unique_address' ignored [-Werror,-Wunknown-attributes]
	# [[no_unique_address]] Predicate predicate;
	add_compile_options(-Wno-unknown-attributes)
endif()

# Clang & GCC diagnostic flags. GCC can't compile this project right now but I am hopeful.
if (CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	add_compile_options(
		-fdiagnostics-color
		-Wall
		-Wextra
		-Wpedantic
	)
	if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
		# Soften common development warnings
		add_compile_options(
			-Wno-unused-but-set-variable
			-Wno-unused-result
		)
	endif()
endif()

# Clang feature flags
if (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	# `std::stop_token` and maybe others
	add_compile_definitions(_LIBCPP_ENABLE_EXPERIMENTAL)
	# This enables `#include <expected>` somehow. This can be removed in Clang 19.1.0.
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 20)
		add_compile_options(-D__cpp_concepts=202002L -Wno-builtin-macro-redefined)
	endif()

	# Debug / Release configuration flags
	if (CMAKE_BUILD_TYPE STREQUAL Debug)
		# Required for clangd w/ modules
		add_compile_options(-fretain-comments-from-system-headers)

		# Enable ASAN / LSAN
		add_compile_options(-fsanitize=address)
		add_link_options(-fsanitize=address -shared-libasan)
	else()
		# clang-18 complains about `std::aligned_storage` within `std::shared_ptr` compiling against
		# libstdc++
		add_compile_options(-Wno-deprecated-declarations)
	endif()
endif()

# Generic abstract C++ utilities
add_subdirectory(packages/utility)

# JavaScript adjacent code which is not specific to v8 or nodejs
add_subdirectory(packages/js/value)

# v8 common packages. These source files are built twice, once which links against nodejs, and
# another which links against internal v8/z8.
# Note: `target_link_libraries` of OBJECT isn't inherited. So ivm_value and ivm_utility must be
# linked to the final target.
# https://gitlab.kitware.com/cmake/cmake/-/issues/18090
set(IV8_PREFIX ivm_nodejs)
set(IV8_INCLUDE_DIR ${NODE_DIST_DIR}/include/node)
add_subdirectory(packages/js/v8 ivm_nodejs_jsv8)
add_subdirectory(packages/third_party/v8 ivm_nodejs_v8)

# Embedded v8
set(IV8_PREFIX ivm_embedded)
set(IV8_INCLUDE_DIR ${V8_INCLUDE_DIR})
set(IVM_IV8_EMBEDDED ON)
set(IVM_IV8_TRICKSHOT ON)
add_subdirectory(packages/js/v8 ivm_embedded_jsv8)
add_subdirectory(packages/third_party/v8 ivm_embedded_v8)

# libivm-v8
add_subdirectory(packages/libivm-v8)

# Napi shim
add_subdirectory(packages/third_party/napi)

# nodejs require-able module
add_subdirectory(packages/js/napi ivm_jsnapi)
add_subdirectory(packages/nodejs)
