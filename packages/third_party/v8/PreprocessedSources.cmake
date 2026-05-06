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
				COMMAND sh -c "${CMAKE_CURRENT_LIST_DIR}/expand_def ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} -D${SEARCH}=${REPLACE} >${GEN_DIR}/${FILE}"
			)
			set_source_files_properties("${GEN_DIR}/${FILE}" PROPERTIES GENERATED TRUE)
			target_sources(${TARGET} ${SCOPE} "${GEN_DIR}/${FILE}")
		endforeach()
	endif()
endfunction()
