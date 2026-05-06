# Instruct `find_package` to find listed npm packages
include_guard(GLOBAL)
set(NPM_INCLUDED_PACKAGE_NAMES CACHE INTERNAL "")
function(find_package_from_npm PACKAGE_NAME)
	# Optional parameter to override the deduplicated package name, allowing multiple includes of the
	# same package.
	set(NPM_PACKAGE_NAME "${PACKAGE_NAME}")
	if(DEFINED ARGV1)
		set(PACKAGE_NAME "${ARGV1}")
	endif()

	# Don't include more than once
	if(PACKAGE_NAME IN_LIST NPM_INCLUDED_PACKAGE_NAMES)
		return()
	endif()
	set(NPM_INCLUDED_PACKAGE_NAMES ${NPM_INCLUDED_PACKAGE_NAMES} "${PACKAGE_NAME}" CACHE INTERNAL "")

	# Search and include
	execute_process(
		COMMAND sh -c "cd \"${CMAKE_CURRENT_SOURCE_DIR}\"; node -e 'console.log(require.resolve(\"${NPM_PACKAGE_NAME}/CMakeLists.txt\"))'"
		OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE PACKAGE_NAME_LOCATION
		RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "Could not find npm package '${NPM_PACKAGE_NAME}'")
	endif()
	get_filename_component(PACKAGE_NAME_LOCATION "${PACKAGE_NAME_LOCATION}" DIRECTORY)
	message(STATUS "npm package '${NPM_PACKAGE_NAME}' found at '${PACKAGE_NAME_LOCATION}'")
	add_subdirectory("${PACKAGE_NAME_LOCATION}" "${CMAKE_BINARY_DIR}/_npm/${PACKAGE_NAME}")
endfunction()
