add_library(ivm_backend_v8 SHARED)
target_sources(ivm_backend_v8
	PRIVATE
		agent.cc
		environment.cc
		main.cc
		module.cc
		script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		include/environment.cc
		include/external.cc
		include/realm.cc
		utility.cc
)
target_embed_content(ivm_backend_v8 runtime/dist/runtime.js)
target_include_directories(ivm_backend_v8 SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
target_link_libraries(ivm_backend_v8 PUBLIC
	ivm_isolated_v8
	ivm_napi
	ivm_nodejs_common_v8
	ivm_nodejs_v8
	ivm_utility
	ivm_value
	ivm_value_napi
)

# Windows linking
if (WIN32)
	target_link_directories(ivm_backend_v8 PUBLIC ${NODE_DIST_DIR}/node/windows/${IVM_HOST_ARCH})
	target_link_libraries(ivm_backend_v8 PUBLIC node dbghelp winmm)
endif()

# Copy out to ./dist
add_custom_command(TARGET ivm_backend_v8 POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
		$<TARGET_FILE:ivm_backend_v8>
		${CMAKE_SOURCE_DIR}/packages/isolated-vm/dist/backend_v8/${IVM_HOST_PLATFORM}-${IVM_HOST_ARCH}/backend_v8.node
)
