add_library(nodejs STATIC)
target_compile_features(nodejs PUBLIC cxx_std_23)

# Set `NAPI_VERSION` macro
if(NOT DEFINED NAPI_VERSION)
	message(FATAL_ERROR "Missing 'NAPI_VERSION' cmake variable.")
endif()
target_compile_definitions(nodejs PUBLIC "NAPI_VERSION=${NAPI_VERSION}")

# Acquire or locate nodejs headers
if(NOT DEFINED NODE_DIST_DIR)
	if(DEFINED ENV{NODE_DIST_DIR})
		set(NODE_DIST_DIR "$ENV{NODE_DIST_DIR}")
		cmake_path(ABSOLUTE_PATH NODE_DIST_DIR BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
		set(NODE_DIST_DIR "${NODE_DIST_DIR}" CACHE STRING "")
	endif()
endif()
if(NOT DEFINED NODE_DIST_DIR)
	include(FetchContent)
	FetchContent_Declare(
		node_headers
		URL https://nodejs.org/download/release/v24.0.0/node-v24.0.0-headers.tar.gz
	)
	FetchContent_MakeAvailable(node_headers)
	set(NODE_DIST_DIR "${node_headers_SOURCE_DIR}" CACHE STRING "")
endif()
if(NOT EXISTS "${NODE_DIST_DIR}/include/node/node.h")
	message(FATAL_ERROR "Missing ${NODE_DIST_DIR}/include/node/node.h")
endif()
message(STATUS "Using Node.js headers from '${NODE_DIST_DIR}'")
target_include_directories(nodejs SYSTEM PUBLIC "${NODE_DIST_DIR}/include/node")

target_sources(nodejs
	PUBLIC FILE_SET CXX_MODULES FILES
		js_native_api_types.cc
		js_native_api.cc
		node_api.cc
		nodejs.cc
		uv.cc
)
