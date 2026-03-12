cmake_minimum_required(VERSION 3.30.0 FATAL_ERROR)

# Project config
project(isolated_vm LANGUAGES C CXX)

# C++26 environment
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use lld (for LTO), and also gc-sections
set(CMAKE_LINKER_TYPE LLD)
add_compile_options(-fdata-sections -ffunction-sections)
add_link_options(-Wl,--icf=all)
if(LINUX)
	# Not supported in ld64 [darwin], or lld-link [windows]
	add_link_options(-Wl,--gc-sections)
endif()

# Global compiler flags
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Release flags
if(CMAKE_BUILD_TYPE STREQUAL Release)
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
	set(CMAKE_C_VISIBILITY_PRESET hidden)
	set(CMAKE_CXX_VISIBILITY_PRESET hidden)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
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
	set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")
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
function(target_embed_content TARGET FILE IDENTIFIER)
	set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/embed")
	set(OUT_FILE "${GEN_DIR}/${IDENTIFIER}")
	file(MAKE_DIRECTORY "${GEN_DIR}")
	target_include_directories(${TARGET} PRIVATE "${GEN_DIR}")

	message(STATUS "Embedding ${FILE} as ${IDENTIFIER}")
	add_custom_command(
		DEPENDS "${FILE}"
		OUTPUT "${OUT_FILE}.c" "${OUT_FILE}.h"
		COMMAND sh -c "${PROJECT_SOURCE_DIR}/embed.sh ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${OUT_FILE} ${IDENTIFIER}"
	)
	set_source_files_properties("${OUT_FILE}.c" PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE "${OUT_FILE}.c")
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
		file(WRITE "${GEN_DIR}/.clang-tidy" "Checks: '-*'")
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


# `find_package_from_npm` from `@auto_js/js`
include(FindNpmPackage)

# Embedded v8
set(IVM_V8_EMBEDDED ON)
set(IVM_V8_TRICKSHOT ON)
add_subdirectory(packages/third_party/v8)

# nodejs require-able module
add_subdirectory(packages/backend_napi_v8)

# Playground
add_subdirectory(playground)
