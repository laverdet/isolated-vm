# Instruct `find_package` to find listed npm packages
include_guard(GLOBAL)

# set_npm_package_properties([variables...])
function(set_npm_package_properties PACKAGE_NAME)
	# Inherit `NPM_PACKAGE_NAME` from `find_package_from_npm` scope
	if(DEFINED NPM_PACKAGE_NAME)
		set(PACKAGE_NAME "${NPM_PACKAGE_NAME}")
	endif()

	# Save variables as global properties
	list(POP_FRONT ARGV) # PACKAGE_NAME
	foreach(VAR IN LISTS ARGV)
		set_property(GLOBAL PROPERTY "NPM_PACKAGE:${PACKAGE_NAME}:${VAR}" "${${VAR}}")
	endforeach()
endfunction()

# find_package_from_npm(name [AS identifier] [PROPAGATE variables...])
function(find_package_from_npm NPM_PACKAGE_NAME)
	# Parse arguments:
	cmake_parse_arguments(PARSE_ARGV 1 npm "" "AS" "PROPAGATE")

	# Optional parameter to override the deduplicated package name, allowing multiple includes of the
	# same package.
	set(NPM_PACKAGE_NAME_REAL "${NPM_PACKAGE_NAME}")
	if(DEFINED npm_AS)
		set(NPM_PACKAGE_NAME "${npm_AS}")
		unset(npm_AS)
	endif()

	# Check against global include list
	get_property(NPM_INCLUDED_PACKAGE_NAMES GLOBAL PROPERTY NPM_INCLUDED_PACKAGE_NAMES)
	if(NOT NPM_PACKAGE_NAME IN_LIST NPM_INCLUDED_PACKAGE_NAMES)
		# It was not included yet. Mark as included, and `add_subdirectory`
		list(APPEND NPM_INCLUDED_PACKAGE_NAMES "${NPM_PACKAGE_NAME}")
		set_property(GLOBAL PROPERTY NPM_INCLUDED_PACKAGE_NAMES ${NPM_INCLUDED_PACKAGE_NAMES})

		# Search and include
		execute_process(
			COMMAND sh -c "cd \"${CMAKE_CURRENT_SOURCE_DIR}\"; node -e 'console.log(require.resolve(\"${NPM_PACKAGE_NAME_REAL}/CMakeLists.txt\"))'"
			OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE PACKAGE_NAME_LOCATION
			RESULT_VARIABLE RESULT)
		if(NOT RESULT STREQUAL 0)
			message(FATAL_ERROR "Could not find npm package '${NPM_PACKAGE_NAME_REAL}'")
		endif()
		get_filename_component(PACKAGE_NAME_LOCATION "${PACKAGE_NAME_LOCATION}" DIRECTORY)
		message(STATUS "npm package '${NPM_PACKAGE_NAME_REAL}' found at '${PACKAGE_NAME_LOCATION}'")
		add_subdirectory("${PACKAGE_NAME_LOCATION}" "${CMAKE_BINARY_DIR}/_npm/${NPM_PACKAGE_NAME}")
	endif()

	# Propagate variables registered by set_npm_package_properties
	foreach(VAR IN LISTS npm_PROPAGATE)
		get_property(VALUE GLOBAL PROPERTY "NPM_PACKAGE:${NPM_PACKAGE_NAME}:${VAR}")
		set(${VAR} "${VALUE}" PARENT_SCOPE)
	endforeach()
	unset(npm_PROPAGATE)
endfunction()
