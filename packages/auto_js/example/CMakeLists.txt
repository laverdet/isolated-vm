cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)
project(example LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(NAPI_VERSION 10)

include(FindNpmPackage)
find_package_from_npm(@auto_js/napi)

add_library(example SHARED)
set_target_properties(example PROPERTIES PREFIX "" SUFFIX ".node")
target_sources(example
	PRIVATE
		main.cc
)
target_link_libraries(example PUBLIC auto_js napi_js nodejs utility_js)
