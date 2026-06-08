# See: ./triplet_util for format
function(triplet_util VARIABLE_NAME FORMAT)
	# npm strips the executable bits, so we add them back
	set(TRIPLET_UTIL "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/triplet_util")
	file(CHMOD "${TRIPLET_UTIL}" PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	execute_process(
		COMMAND sh -c "${TRIPLET_UTIL} ${FORMAT}"
		OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE TRIPLET
		RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "triplet_util failed." )
		message(STATUS "$TRIPLET")
	endif()
	set(${VARIABLE_NAME} "${TRIPLET}" PARENT_SCOPE)
endfunction()
