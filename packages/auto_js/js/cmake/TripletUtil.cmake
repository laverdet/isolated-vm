# See: ./triplet_util for format
function(triplet_util VARIABLE_NAME FORMAT)
	execute_process(
		COMMAND sh -c "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/triplet_util ${FORMAT}"
		OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE TRIPLET
		RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "triplet_util failed." )
		message(STATUS "$TRIPLET")
	endif()
	set(${VARIABLE_NAME} "${TRIPLET}" PARENT_SCOPE)
endfunction()
