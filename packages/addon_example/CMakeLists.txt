# import std;
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
else()
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
endif()

# project settings
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 26)
cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)
project(addon_example LANGUAGES CXX)

# language settings
set(CMAKE_CXX_MODULE_STD ON)

# compilation settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
	set(CMAKE_SHARED_LINKER_FLAGS "-dynamiclib -undefined dynamic_lookup")
endif()

# napi configuration
include(FindNpmPackage)
find_package_from_npm(@isolated-vm/addon)

# target
add_library(addon_example SHARED)
set_target_properties(addon_example PROPERTIES PREFIX "")
target_link_libraries(addon_example PUBLIC isolated_vm)

# sources
target_sources(addon_example
	PRIVATE
		main.cc
)
