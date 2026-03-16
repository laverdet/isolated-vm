cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)
project(example LANGUAGES CXX)

# language settings
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# compilation settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
	set(CMAKE_SHARED_LINKER_FLAGS "-dynamiclib -undefined dynamic_lookup")
endif()

# napi configuration
include(FindNpmPackage)
set(NAPI_VERSION 10)
find_package_from_npm(@auto_js/napi)

# target
add_library(example SHARED)
set_target_properties(example PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(example PUBLIC auto_js napi_js nodejs utility_js)

# sources
target_sources(example
	PRIVATE
		main.cc
)
