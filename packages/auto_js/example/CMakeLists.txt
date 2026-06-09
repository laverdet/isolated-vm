# `import std` configuration (before project)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_EXTENSIONS OFF)
cmake_minimum_required(VERSION 4.3.0 FATAL_ERROR)

# project & language settings
project(example LANGUAGES CXX)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# compilation settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
	set(CMAKE_SHARED_LINKER_FLAGS "-dynamiclib -undefined dynamic_lookup")
endif()

# napi configuration
include(FindNpmPackage)
set(NAPI_VERSION 10)
find_package_from_npm(@auto_js/napi PROPAGATE auto_js napi_js)

# target
add_library(example SHARED)
set_target_properties(example PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(example PRIVATE ${auto_js} ${napi_js} utility_js)

# sources
target_sources(example
	PRIVATE
		main.cc
)
