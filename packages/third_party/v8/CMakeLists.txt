# Library name: google_v8, or nodejs_v8
if(NOT DEFINED V8_VENDOR)
	set(V8_VENDOR "google")
endif()
set(NAME "${V8_VENDOR}_v8")
add_library("${NAME}" STATIC)

if (V8_EMBEDDED)
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
else()
	# Header files (nodejs)
	if(NOT EXISTS "${V8_INCLUDE_DIR}/v8.h")
		message(FATAL_ERROR "Missing ${V8_INCLUDE_DIR}/v8.h")
	endif()
	message(STATUS "Using v8 headers from '${V8_INCLUDE_DIR}'")
endif()

# Preprocessor macros which enable dual v8 in the same process
if(V8_TRICKSHOT)
	set(MODULE_NAME z8)
	target_compile_definitions("${NAME}" PUBLIC
		cppgc=zppgc
		unibrow=znibrow
		v8_crdtp=z8_crdtp
		V8_Dcheck=Z8_Dcheck
		V8_Fatal=Z8_Fatal
		v8_inspector=z8_inspector
		v8=z8
	)
else()
	set(MODULE_NAME v8)
endif()

# version.h
target_include_directories("${NAME}" SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# v8 headers
target_include_directories("${NAME}" SYSTEM PRIVATE "${V8_INCLUDE_DIR}")

if(V8_EMBEDDED)
	# Include v8-gn.h
	target_compile_options("${NAME}" PRIVATE -include "${V8_INCLUDE_GN_FILE}")

	# Include deeper v8 headers not provided by nodejs
	target_compile_definitions("${NAME}" PRIVATE INCLUDE_V8_PLATFORM)

	# Link against v8, which is built elsewhere
	if(V8_LIBRARY_TYPE STREQUAL static)
		target_link_directories("${NAME}" PUBLIC "${V8_LIBRARY_PATH}/obj")
		target_link_libraries("${NAME}" PUBLIC v8_monolith)

		# Building v8 against libstdc++ w/ clang requires `-latomic`
		# TODO(?): We would need to record how v8 was compiled, not which toolchain is compiling *this*
		# project.
		# if(GLIBCXX)
		# 	target_link_libraries(google_v8 PUBLIC atomic)
		# endif()
	elseif(V8_LIBRARY_TYPE STREQUAL shared)
		target_link_directories("${NAME}" PUBLIC "${V8_LIBRARY_PATH}")
		target_link_libraries("${NAME}" PUBLIC v8 v8_libbase v8_libplatform icuuc)
	else()
		message(FATAL_ERROR "Unknown v8 library type")
	endif()
endif()

# Sources which will be preprocessed by `expand_def`
target_preprocessed_sources("${NAME}" v8 "${MODULE_NAME}"
	PUBLIC FILE_SET CXX_MODULES FILES
		v8.cc
)
