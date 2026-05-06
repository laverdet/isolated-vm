# Library name: google_v8, or nodejs_v8
set(CXX_MODULE_NAME v8)
if(PACKAGE_NAME STREQUAL "@isolated-vm/embedded_v8")
	set(TARGET_NAME "google_v8")
elseif(PACKAGE_NAME STREQUAL "@auto_js/v8_exports")
	set(TARGET_NAME "nodejs_v8")
else()
	message(FATAL_ERROR "Missing or unknown @auto_js/v8 PACKAGE_NAME: '${PACKAGE_NAME}'")
endif()
add_library("${TARGET_NAME}" STATIC)

if(TARGET_NAME STREQUAL "google_v8")
	# Header files (embedded)
	if(NOT DEFINED V8_INCLUDE_DIR)
		set(V8_INCLUDE_DIR "$ENV{V8_BUNDLE_PATH}/../include")
		cmake_path(ABSOLUTE_PATH V8_INCLUDE_DIR BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
		cmake_path(NORMAL_PATH V8_INCLUDE_DIR)
		set(V8_INCLUDE_DIR "${V8_INCLUDE_DIR}" CACHE STRING "")
	endif()
	if(NOT EXISTS "${V8_INCLUDE_DIR}/v8.h")
		message(FATAL_ERROR "Missing ${V8_INCLUDE_DIR}/v8.h")
	endif()
	message(STATUS "Using v8 headers from '${V8_INCLUDE_DIR}'")

	# v8-gn.h, build configuration
	if(NOT DEFINED V8_INCLUDE_GN_FILE)
		set(V8_INCLUDE_GN_FILE "$ENV{V8_BUNDLE_PATH}/include/v8-gn.h")
		cmake_path(ABSOLUTE_PATH V8_INCLUDE_GN_FILE BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
		cmake_path(NORMAL_PATH V8_INCLUDE_GN_FILE)
		set(V8_INCLUDE_GN_FILE "${V8_INCLUDE_GN_FILE}" CACHE STRING "")
	endif()
	if(NOT EXISTS "${V8_INCLUDE_GN_FILE}")
		message(FATAL_ERROR "Missing v8-gn.h")
	endif()
	message(STATUS "Using v8-gn.h from '${V8_INCLUDE_GN_FILE}'")

	# v8 pre-built library
	if(NOT DEFINED V8_LIBRARY_PATH)
		set(V8_LIBRARY_PATH "$ENV{V8_BUNDLE_PATH}")
		cmake_path(ABSOLUTE_PATH V8_LIBRARY_PATH BASE_DIRECTORY "${CMAKE_SOURCE_DIR}")
		cmake_path(NORMAL_PATH V8_LIBRARY_PATH)
		set(V8_LIBRARY_PATH "${V8_LIBRARY_PATH}" CACHE STRING "")
	endif()
	if(NOT EXISTS "${V8_LIBRARY_PATH}")
		message(FATAL_ERROR "Missing v8 library")
	endif()
	message(STATUS "Using v8 library path '${V8_LIBRARY_PATH}'")

	# v8 library type (static, shared)
	if(NOT DEFINED V8_LIBRARY_TYPE)
		set(V8_LIBRARY_TYPE "$ENV{V8_LIBRARY_TYPE}" CACHE STRING "")
	endif()
	message(STATUS "Using v8 library type '${V8_LIBRARY_TYPE}'")

	# Preprocessor macros which enable dual v8 in the same process
	if(V8_TRICKSHOT)
		set(MODULE_NAME z8)
		target_compile_definitions("${TARGET_NAME}" PUBLIC
			cppgc=zppgc
			unibrow=znibrow
			v8_crdtp=z8_crdtp
			V8_Dcheck=Z8_Dcheck
			V8_Fatal=Z8_Fatal
			v8_inspector=z8_inspector
			v8=z8
		)
	endif()
else()
	# Header files (nodejs)
	if(NOT DEFINED NODE_DIST_DIR)
		message(FATAL_ERROR "'NODE_DIST_DIR' is not set")
	endif()
	set(V8_INCLUDE_DIR "${NODE_DIST_DIR}/include/node")
	if(NOT EXISTS "${V8_INCLUDE_DIR}/v8.h")
		message(FATAL_ERROR "Missing '${V8_INCLUDE_DIR}/v8.h'")
	endif()
	message(STATUS "Using v8 headers from '${V8_INCLUDE_DIR}'")
endif()

# version.h
target_include_directories("${TARGET_NAME}" SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# v8 headers
target_include_directories("${TARGET_NAME}" SYSTEM PRIVATE "${V8_INCLUDE_DIR}")

if(TARGET_NAME STREQUAL "google_v8")
	# Include v8-gn.h
	target_compile_options("${TARGET_NAME}" PRIVATE -include "${V8_INCLUDE_GN_FILE}")

	# Include deeper v8 headers not provided by nodejs
	target_compile_definitions("${TARGET_NAME}" PRIVATE INCLUDE_V8_PLATFORM)

	# Link against v8, which is built elsewhere
	if(V8_LIBRARY_TYPE STREQUAL static)
		target_link_directories("${TARGET_NAME}" PUBLIC "${V8_LIBRARY_PATH}/obj")
		target_link_libraries("${TARGET_NAME}" PUBLIC v8_monolith)

		# Building v8 against libstdc++ w/ clang requires `-latomic`
		# TODO(?): We would need to record how v8 was compiled, not which toolchain is compiling *this*
		# project.
		# if(GLIBCXX)
		# 	target_link_libraries(google_v8 PUBLIC atomic)
		# endif()
	elseif(V8_LIBRARY_TYPE STREQUAL shared)
		target_link_directories("${TARGET_NAME}" PUBLIC "${V8_LIBRARY_PATH}")
		target_link_libraries("${TARGET_NAME}" PUBLIC v8 v8_libbase v8_libplatform icuuc)
	else()
		message(FATAL_ERROR "Unknown v8 library type")
	endif()
endif()

# Sources which will be preprocessed by `expand_def`
include(./PreprocessedSources.cmake)
target_preprocessed_sources("${TARGET_NAME}" v8 "${CXX_MODULE_NAME}"
	PUBLIC FILE_SET CXX_MODULES FILES
		v8.cc
)
