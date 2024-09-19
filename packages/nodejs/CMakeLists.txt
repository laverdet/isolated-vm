add_library(ivm_backend_v8 SHARED)
target_sources(ivm_backend_v8
	PRIVATE
		agent.cc
		environment.cc
		main.cc
		script.cc
		value/arguments.cc
		value/array.cc
		value/object.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		include/_module.cc
		include/arguments.cc
		include/array.cc
		include/environment.cc
		include/external.cc
		include/object.cc
		utility.cc
		value/accept.cc
		value/visit.cc
)
target_compile_definitions(ivm_backend_v8 PRIVATE
	NODE_ADDON_API_DISABLE_DEPRECATED
)
target_include_directories(ivm_backend_v8 SYSTEM PRIVATE
	${NAPI_INCLUDE_DIR}
	${NODE_DIST_DIR}/include
)
target_link_libraries(ivm_backend_v8 PUBLIC
	ivm_isolated_v8
	ivm_napi
	ivm_nodejs_common_v8
	ivm_nodejs_v8
	ivm_utility
	ivm_value
)

# Windows linking
if (WIN32)
	target_link_directories(ivm_backend_v8 PUBLIC ${NODE_DIST_DIR}/windows/${IVM_HOST_ARCH})
	target_link_libraries(ivm_backend_v8 PUBLIC node dbghelp winmm)
endif()

# Copy out to ./dist
add_custom_command(TARGET ivm_backend_v8 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		$<TARGET_FILE:ivm_backend_v8>
		${CMAKE_SOURCE_DIR}/dist/backend_v8/${IVM_HOST_PLATFORM}-${IVM_HOST_ARCH}/backend_v8.node
)
