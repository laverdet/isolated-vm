function(mach_o_make_weak TARGET)
	if(APPLE)
		cmake_parse_arguments(MACH_O "" "" SYMBOLS ${ARGN})
		if(NOT MACH_O_SYMBOLS)
			message(FATAL_ERROR "mach_o_make_weak(${TARGET}): SYMBOLS list is required")
		endif()

		find_package(Python3 REQUIRED COMPONENTS Interpreter)

		set(_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mach_o_make_weak.py")
		if(NOT EXISTS "${_script}")
			message(FATAL_ERROR "mach_o_make_weak: script not found at ${_script}")
		endif()

		add_custom_command(TARGET ${TARGET} POST_BUILD
			COMMAND ${Python3_EXECUTABLE} "${_script}" "$<TARGET_FILE:${TARGET}>" ${MACH_O_SYMBOLS}
			COMMAND codesign --force --sign - "$<TARGET_FILE:${TARGET}>"
			VERBATIM
			COMMENT "Weakening Mach-O imports in ${TARGET}")
	endif()
endfunction()
