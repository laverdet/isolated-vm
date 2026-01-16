cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)

# Location of nodejs header files
if(NOT DEFINED NODE_DIST_DIR)
	set(NODE_DIST_DIR $ENV{NODE_DIST_DIR})
endif()
if(NOT EXISTS "${NODE_DIST_DIR}/include/node/node.h")
	message(FATAL_ERROR "Missing ${NODE_DIST_DIR}/include/node/node.h")
endif()

# Location of v8 header files
if(NOT DEFINED V8_INCLUDE_DIR)
	set(V8_INCLUDE_DIR $ENV{V8_INCLUDE_DIR})
endif()
if(NOT EXISTS "${V8_INCLUDE_DIR}/v8.h")
	message(FATAL_ERROR "Missing ${V8_INCLUDE_DIR}/v8.h")
endif()

# Location of v8-gn.h, build configuration
if(NOT DEFINED V8_INCLUDE_GN_FILE)
	set(V8_INCLUDE_GN_FILE $ENV{V8_INCLUDE_GN_FILE})
endif()
if(NOT EXISTS "${V8_INCLUDE_GN_FILE}")
	message(FATAL_ERROR "Missing v8-gn.h")
endif()

# v8 library type (static, shared)
if(NOT DEFINED V8_LIBRARY_TYPE)
	set(V8_LIBRARY_TYPE $ENV{V8_LIBRARY_TYPE})
endif()

# v8 pre-built library location
if(NOT DEFINED V8_LIBRARY_PATH)
	set(V8_LIBRARY_PATH $ENV{V8_LIBRARY_PATH})
endif()
if(NOT EXISTS "${V8_LIBRARY_PATH}")
	message(FATAL_ERROR "Missing v8 library")
endif()

# Ensure absolute path for variables used in subdirectories
cmake_path(ABSOLUTE_PATH NODE_DIST_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_INCLUDE_DIR BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_INCLUDE_GN_FILE BASE_DIRECTORY ${CMAKE_SOURCE_DIR})
cmake_path(ABSOLUTE_PATH V8_LIBRARY_PATH BASE_DIRECTORY ${CMAKE_SOURCE_DIR})

# Project config
project(isolated_vm LANGUAGES C CXX)

# Boost requirement
find_package(Boost 1.83 REQUIRED)

# C++26 environment
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler flags
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# No warnings allowed in release
if(CMAKE_BUILD_TYPE STREQUAL Release)
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()

# Check libc++ / libstdc++
# https://stackoverflow.com/questions/42593022/cmake-detect-which-library-libc-or-libstdc-is-configured-to-be-used-against
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__GLIBCXX__ version GLIBCXX)
check_cxx_symbol_exists(_LIBCPP_VERSION version LIBCPP)

# Build with libc++ if you want
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")

# Apple dylib flags
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
	set(CMAKE_SHARED_LINKER_FLAGS "-dynamiclib -undefined dynamic_lookup")
endif()

# Microsoft Visual C++. It can't actually compile the templates in `js`, but this would be required
# if it could.
if(MSVC)
	# Lmfao: "After 20+ years, we’re finally able to set the value for “__cplusplus” macro forward in
	# the MSVC compiler."
	# https://developercommunity.visualstudio.com/t/msvc-incorrectly-defines-cplusplus/139261#T-N223868
	# https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
	add_compile_options(/Zc:__cplusplus)
endif()

# Windows flags. You want /MT and not /MD
if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL Debug)
	set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

# Windows Clang flags
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	# error: unknown attribute 'no_unique_address' ignored [-Werror,-Wunknown-attributes]
	# [[no_unique_address]] Predicate predicate;
	add_compile_options(-Wno-unknown-attributes)
endif()

# Clang & GCC diagnostic flags. GCC can't compile this project right now but I am hopeful.
if(CMAKE_CXX_COMPILER_ID STREQUAL Clang OR CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	add_compile_options(
		-fdiagnostics-color
		-Wall
		-Wextra
		-Wpedantic
	)
	if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
		# Soften common development warnings
		add_compile_options(
			-Wno-unused-but-set-variable
			-Wno-unused-result
		)
	endif()
endif()

# Clang feature flags
if(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	# Ignore warnings from `expand_def` using gnu c-preprocessor against clang compiler.
	add_compile_options(-Wno-gnu-line-marker)
	# Debug / Release configuration flags
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		# Enable (A|L|UB)SAN
		add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fsanitize=address,undefined>)
		add_link_options(-fsanitize=address,undefined -shared-libasan)
	else()
		# clang-18 complains about `std::aligned_storage` within `std::shared_ptr` compiling against
		# libstdc++
		add_compile_options(-Wno-deprecated-declarations)
		# Unused parameters in assert(..)
		add_compile_options(-Wno-unused-parameter)
	endif()
endif()

# Convert file into a binary blob and embed it into a C++ source file.
function(target_embed_content TARGET)
	set(FILE_SET ${ARGV})
	list(POP_FRONT FILE_SET)
	set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/embed")
	file(MAKE_DIRECTORY "${GEN_DIR}")
	target_include_directories(${TARGET} PRIVATE "${GEN_DIR}")
	foreach(FILE ${FILE_SET})
		set(IDENTIFIER "${FILE}")
		string(REPLACE . _ IDENTIFIER "${IDENTIFIER}")
		string(REPLACE / _ IDENTIFIER "${IDENTIFIER}")
		message(STATUS "Embedding ${FILE} as ${IDENTIFIER}")
		add_custom_command(
			DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
			OUTPUT "${GEN_DIR}/${FILE}.c" "${GEN_DIR}/${FILE}.h"
			COMMAND sh -c "${PROJECT_SOURCE_DIR}/embed.sh ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${GEN_DIR}/${FILE} ${IDENTIFIER}"
		)
		set_source_files_properties("${GEN_DIR}/${FILE}.c" PROPERTIES GENERATED TRUE)
		target_sources(${TARGET} PRIVATE "${GEN_DIR}/${FILE}.c")
	endforeach()
endfunction()

# Generate lightly preprocessed C++ source files, to workaround P3034R1
function(target_preprocessed_sources TARGET SEARCH REPLACE)
	set(FILE_SET ${ARGV})
	list(POP_FRONT FILE_SET) # TARGET
	list(POP_FRONT FILE_SET) # SEARCH
	list(POP_FRONT FILE_SET) # REPLACE

	if("${SEARCH}" STREQUAL "${REPLACE}")
		# No-op replacement directly uses original sources
		target_sources(${TARGET} ${FILE_SET})
	else()
		# Generated sources with macro replacement
		set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.${REPLACE}")
		file(MAKE_DIRECTORY "${GEN_DIR}")
		set(SCOPE)
		set(NEXT_SCOPE)
		foreach(FILE ${FILE_SET})
			# Uppercase "files" are actually scope keywords
			if("${FILE}" MATCHES "^[A-Z_]+$")
				list(APPEND NEXT_SCOPE "${FILE}")
				continue()
			elseif(NEXT_SCOPE)
				set(SCOPE ${NEXT_SCOPE})
				set(NEXT_SCOPE)
			endif()
			# Add generator command
			add_custom_command(
				DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
				OUTPUT "${GEN_DIR}/${FILE}"
				COMMAND sh -c "${PROJECT_SOURCE_DIR}/scripts/cmake/expand_def ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} -D${SEARCH}=${REPLACE} >${GEN_DIR}/${FILE}"
			)
			set_source_files_properties("${GEN_DIR}/${FILE}" PROPERTIES GENERATED TRUE)
			target_sources(${TARGET} ${SCOPE} "${GEN_DIR}/${FILE}")
		endforeach()
	endif()
endfunction()

# Setup `#include "shim/[..]"`
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/packages")

# Generic abstract C++ utilities
add_subdirectory(packages/utility)

# Napi shim
add_subdirectory(packages/third_party/nodejs)

# Embedded v8
set(IVM_V8_EMBEDDED ON)
set(IVM_V8_TRICKSHOT ON)
add_subdirectory(packages/third_party/v8)

# auto_js packages
add_subdirectory(packages/auto_js/js)
add_subdirectory(packages/auto_js/napi)
add_subdirectory(packages/auto_js/v8)

# nodejs require-able module
add_subdirectory(packages/backend_napi_v8)

# Playground
add_subdirectory(playground)
