message(STATUS "Cmake version: ${CMAKE_VERSION}")

# Mess of `import std` configuration
if(WIN32)
	cmake_minimum_required(VERSION 4.3.0 FATAL_ERROR)
	set(CMAKE_CXX_STDLIB_MODULES_JSON ms-stl.modules.json)
elseif(APPLE)
	cmake_minimum_required(VERSION 4.0.0 FATAL_ERROR)
	execute_process(COMMAND brew --prefix llvm OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE BREW_LLVM_LOCATION RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "Failed to run brew")
	endif()
	set(CMAKE_CXX_STDLIB_MODULES_JSON "${BREW_LLVM_LOCATION}/lib/c++/libc++.modules.json")
else()
	cmake_minimum_required(VERSION 4.0.0 FATAL_ERROR)
endif()
add_compile_options(
	$<$<CXX_COMPILER_ID:Clang>:-fno-modules-reduced-bmi>
	$<$<CXX_COMPILER_ID:Clang>:-Wno-include-angled-in-module-purview>
	$<$<CXX_COMPILER_ID:Clang>:-Wno-reserved-module-identifier>
)

if(TEST_STDLIB STREQUAL libc++)
	# Build with libc++ if you want
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
elseif(TEST_STDLIB STREQUAL "stl")
	# Or test compilation against STL
	# ninja -C build-stl backend_napi_v8
	add_compile_options(--target=x86_64-pc-windows -Wno-character-conversion -Wno-language-extension-token -Wno-nested-anon-types -Wno-extra-semi -Wno-ignored-attributes)
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
	include_directories(/root/windows/include /root/windows/shared /root/windows/ucrt /root/windows/um /root/windows/boost)
	set(CMAKE_CXX_STDLIB_MODULES_JSON "/root/windows/stl.modules.json")
	set(COMPILE_ONLY ON)
elseif(DEFINED TEST_STDLIB)
	message(FATAL_ERROR "Invalid value for TEST_STDLIB: ${TEST_STDLIB}")
endif()

# C++26 environment
set(CMAKE_CXX_STANDARD 26)
if(COMPILE_ONLY)
	set(CMAKE_CXX_EXTENSIONS OFF)
else()
	set(CMAKE_CXX_EXTENSIONS ON)
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.3.0")
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
else()
	set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
endif()

# Project config
project(isolated_vm LANGUAGES C CXX)
message(STATUS "C++ Standard Library: ${CMAKE_CXX_STANDARD_LIBRARY}")

# import std;
set(CMAKE_CXX_MODULE_STD ON)

# Use lld (for LTO), and also gc-sections
set(CMAKE_LINKER_TYPE LLD)
add_compile_options(-fdata-sections -ffunction-sections)
add_link_options(-Wl,--icf=all)
if(LINUX)
	# Not supported in ld64 [darwin], or lld-link [windows]
	add_link_options(-Wl,--gc-sections)
endif()

# Global compiler flags
if(NOT COMPILE_ONLY)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Release flags
if(CMAKE_BUILD_TYPE STREQUAL Release)
	set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
	set(CMAKE_C_VISIBILITY_PRESET hidden)
	set(CMAKE_CXX_VISIBILITY_PRESET hidden)
	# LTO broken in gcc
	# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=123970
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# Broken diagnostic
	# /workspace/packages/auto_js/v8/visit.cc:261:57: error: ‘visit_and_accept’ may be used uninitialized [-Werror=maybe-uninitialized]
	#   261 | return immediate(subject.As<v8::Object>(), accept);
	#       |        ~~~~~~~~~^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	add_compile_options(-Wno-maybe-uninitialized)
	# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56958
	# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124977
	# /workspace/packages/auto_js/js/transfer/transfer.cc:141:20: warning: structured binding declaration set but not used [-Wunused-but-set-variable=]
	#   141 | const auto [... unique_types ] = util::pack_unique(accept_types..., visit_types...);
	#       |            ^~~~~~~~~~~~~~~~~~~
	add_compile_options(-Wno-unused -Wno-unused-but-set-parameter -Wno-unused-but-set-variable)
endif()

# Check libc++ / libstdc++
# https://stackoverflow.com/questions/42593022/cmake-detect-which-library-libc-or-libstdc-is-configured-to-be-used-against
include(CheckCXXSymbolExists)
check_cxx_symbol_exists(__GLIBCXX__ version GLIBCXX)
check_cxx_symbol_exists(_LIBCPP_VERSION version LIBCPP)

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
	add_compile_options(-fdiagnostics-color -Wall -Wextra)
	if(NOT COMPILE_ONLY)
		add_compile_options(-Wpedantic)
	endif()
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

# Mark exported symbols as exported
set(EXPORT_IS_EXPORT ON)
add_definitions(-DEXPORT_IS_EXPORT)

# Embedded v8
set(V8_EMBEDDED ON)
set(V8_TRICKSHOT ON)
add_subdirectory(packages/third_party/v8 google_v8)

# nodejs v8
set(V8_VENDOR nodejs)
set(V8_EMBEDDED OFF)
set(V8_TRICKSHOT OFF)
add_subdirectory(packages/third_party/v8 nodejs_v8)

# addon api
add_subdirectory(packages/isolated-vm/addon)

# nodejs require-able module
add_subdirectory(packages/backend_napi_v8)

# Playground
add_subdirectory(playground)
