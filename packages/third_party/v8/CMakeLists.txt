# Library name: google_v8, or nodejs_v8
if(NPM_PACKAGE_NAME STREQUAL @auto_js/isolated_v8_exports)
	set(google_v8 embedded_v8)
else()
	set(google_v8 nodejs_v8)
endif()
add_library(${google_v8} STATIC)
set_npm_package_properties(@auto_js/v8_exports google_v8)
target_compile_features(${google_v8} PUBLIC cxx_std_23)

if(NPM_PACKAGE_NAME STREQUAL @auto_js/isolated_v8_exports)
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
	target_compile_options(${google_v8} PRIVATE -include "${V8_INCLUDE_GN_FILE}")

	# Include deeper v8 headers not provided by nodejs
	target_compile_definitions(${google_v8} PRIVATE INCLUDE_V8_PLATFORM)

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
	if(NOT DEFINED V8_TRICKSHOT)
		if(DEFINED ENV{V8_TRICKSHOT})
			set(V8_TRICKSHOT "$ENV{V8_TRICKSHOT}" CACHE STRING "")
		else()
			# nb: For older bundles where it was not defined
			set(V8_TRICKSHOT blam)
		endif()
	endif()
	if(V8_TRICKSHOT)
		target_compile_definitions(${google_v8} PUBLIC
			cppgc=yppgc
			unibrow=ynibrow
			v8_crdtp=y8_crdtp
			V8_Dcheck=Y8_Dcheck
			V8_Fatal=Y8_Fatal
			v8_inspector=y8_inspector
			v8=y8
		)
	endif()

	# P3034R1 workaround for gcc
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(v8 y8)
	else()
		set(v8 v8)
	endif()

	# Link against v8, which is built elsewhere
	if(V8_LIBRARY_TYPE STREQUAL static)
		target_link_directories(${google_v8} PUBLIC "${V8_LIBRARY_PATH}/obj")
		target_link_libraries(${google_v8} PRIVATE v8_monolith)
		if(WIN32)
			target_link_libraries(${google_v8} PRIVATE dbghelp)
		endif()

		# Building v8 against libstdc++ w/ clang requires `-latomic`
		# TODO(?): We would need to record how v8 was compiled, not which toolchain is compiling *this*
		# project.
		# if(GLIBCXX)
		# 	target_link_libraries(google_v8 PRIVATE atomic)
		# endif()
	elseif(V8_LIBRARY_TYPE STREQUAL shared)
		target_link_directories(${google_v8} PRIVATE "${V8_LIBRARY_PATH}")
		target_link_libraries(${google_v8} PRIVATE v8 v8_libbase v8_libplatform icuuc)
	else()
		message(FATAL_ERROR "Unknown v8 library type")
	endif()
else()
	set(v8 v8)
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

# These need to be public, for version detection. I was hoping I could do it with constexpr but
# apparently not:
# https://github.com/llvm/llvm-project/issues/203372

# version.h
target_include_directories(${google_v8} SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# v8 headers
target_include_directories(${google_v8} SYSTEM PUBLIC "${V8_INCLUDE_DIR}")

# Sources which may be preprocessed by `expand_def`
include(./PreprocessedSources.cmake)
target_preprocessed_sources(${google_v8} v8 ${v8}
	PUBLIC FILE_SET CXX_MODULES FILES
		v8.cc
)
