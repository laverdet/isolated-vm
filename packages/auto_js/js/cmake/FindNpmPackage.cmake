# Instruct `find_package` to find listed npm packages
include_guard(GLOBAL)
set(NPM_INCLUDED_PACKAGES CACHE INTERNAL "")
function(find_package_from_npm PACKAGE)
	# Don't include more than once
	if(PACKAGE IN_LIST NPM_INCLUDED_PACKAGES)
		return()
	endif()
	set(NPM_INCLUDED_PACKAGES ${NPM_INCLUDED_PACKAGES} "${PACKAGE}" CACHE INTERNAL "")

	# Search and include
	execute_process(
		COMMAND sh -c "cd \"${CMAKE_CURRENT_SOURCE_DIR}\"; node -e 'console.log(require.resolve(\"${PACKAGE}/CMakeLists.txt\"))'"
		OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE PACKAGE_LOCATION
		RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "Could not find npm package '${PACKAGE}'")
	endif()
	get_filename_component(PACKAGE_LOCATION "${PACKAGE_LOCATION}" DIRECTORY)
	message(STATUS "npm package '${PACKAGE}' found at '${PACKAGE_LOCATION}'")
	add_subdirectory("${PACKAGE_LOCATION}" "${CMAKE_BINARY_DIR}/_npm/${PACKAGE}")
endfunction()
