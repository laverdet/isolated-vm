# Embed arbitrary file content into a compiled target.
include_guard(GLOBAL)

# target_embed_content(<target> <file> <identifier>)
#
# Converts `file` (relative to the current source directory) into a null-terminated character array
# which is compiled into `target` as a C source, so the C language must be enabled. The array is
# declared in a generated header:
#
#   #include "<identifier>.h"
#   extern "C" const char <identifier>[];
function(target_embed_content TARGET FILE IDENTIFIER)
	set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/embed")
	set(OUT_FILE "${GEN_DIR}/${IDENTIFIER}")
	file(MAKE_DIRECTORY "${GEN_DIR}")
	target_include_directories(${TARGET} PRIVATE "${GEN_DIR}")

	message(STATUS "Embedding ${FILE} as ${IDENTIFIER}")
	add_custom_command(
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
		OUTPUT "${OUT_FILE}.c" "${OUT_FILE}.h"
		COMMAND sh -c "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed.sh ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${OUT_FILE} ${IDENTIFIER}"
	)
	set_source_files_properties("${OUT_FILE}.c" PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE "${OUT_FILE}.c")
endfunction()
