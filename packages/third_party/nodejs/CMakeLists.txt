add_library(nodejs STATIC)
target_compile_features(nodejs PUBLIC cxx_std_23)

# Set `NAPI_VERSION` macro
if(NOT DEFINED NAPI_VERSION)
	message(FATAL_ERROR "Missing 'NAPI_VERSION' cmake variable.")
elseif(NAPI_VERSION STREQUAL EXPERIMENTAL)
	target_compile_definitions(nodejs PUBLIC NAPI_EXPERIMENTAL)
else()
	target_compile_definitions(nodejs PUBLIC NAPI_VERSION=${NAPI_VERSION})
endif()

# Architecture, for window linkage
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} NODE_ARCH)
if(NODE_ARCH STREQUAL amd64)
	set(NODE_ARCH x64)
endif()

# Acquire or locate nodejs headers
if(NOT DEFINED NODE_DIST_DIR)
	if(DEFINED ENV{NODE_DIST_DIR})
		set(NODE_DIST_DIR "$ENV{NODE_DIST_DIR}")
		cmake_path(ABSOLUTE_PATH NODE_DIST_DIR BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
		set(NODE_DIST_DIR "${NODE_DIST_DIR}" CACHE STRING "")
	endif()
endif()
if(NOT DEFINED NODE_DIST_DIR)
	if(DEFINED NODE_VERSION)
		message(STATUS "Using Node.js version '${NODE_VERSION}'")
	else()
		execute_process(COMMAND sh -c "node -v | cut -c2-" OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE NODE_VERSION RESULT_VARIABLE RESULT)
		if(NOT RESULT STREQUAL 0)
			message(FATAL_ERROR "nodejs version not set" )
		endif()
		message(STATUS "Detected Node.js version '${NODE_VERSION}'")
	endif()
	set(NODE_HEADERS_URL "https://nodejs.org/download/release/v${NODE_VERSION}/node-v${NODE_VERSION}-headers.tar.gz")
	message(STATUS "Downloading Node.js headers from '${NODE_HEADERS_URL}'")
	include(FetchContent)
	# So much searching to find this 'DOWNLOAD_NO_PROGRESS' option. It's not documented under
	# FetchContent, oh no this is actually an ExternalProject option, you see.
	FetchContent_Declare(node_headers URL "${NODE_HEADERS_URL}" DOWNLOAD_NO_PROGRESS TRUE)
	FetchContent_MakeAvailable(node_headers)
	set(NODE_DIST_DIR "${node_headers_SOURCE_DIR}" CACHE STRING "")
	if(WIN32)
		file(DOWNLOAD "https://nodejs.org/download/release/v${NODE_VERSION}/win-${NODE_ARCH}/node.lib" "${NODE_DIST_DIR}/win-${NODE_ARCH}/node.lib")
	endif()
endif()
if(NOT EXISTS "${NODE_DIST_DIR}/include/node/node.h")
	message(FATAL_ERROR "Missing '${NODE_DIST_DIR}/include/node/node.h'")
endif()
message(STATUS "Using Node.js headers from '${NODE_DIST_DIR}'")

# Configuration
target_include_directories(nodejs SYSTEM PUBLIC "${NODE_DIST_DIR}/include/node")
if(WIN32)
	if(EXISTS "${NODE_DIST_DIR}/win-${NODE_ARCH}/node.lib")
		target_link_directories(nodejs PUBLIC "${NODE_DIST_DIR}/win-${NODE_ARCH}")
	else()
		message(WARNING "Missing '${NODE_DIST_DIR}/win-${NODE_ARCH}/node.lib'")
	endif()
	# nb: `winmm` for `js_clock::now()`
	target_link_libraries(nodejs PRIVATE node winmm)
endif()

# Sources
target_sources(nodejs
	PUBLIC FILE_SET CXX_MODULES FILES
		js_native_api_types.cc
		js_native_api.cc
		node_api_types.cc
		node_api.cc
		nodejs.cc
		uv.cc
)
