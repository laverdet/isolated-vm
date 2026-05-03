find_package_from_npm(@auto_js/js)
find_package_from_npm(@auto_js/utility)

add_library(isolated_vm OBJECT)
target_link_libraries(isolated_vm PRIVATE auto_js utility_js)
target_sources(isolated_vm
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		handle/bound_value.cc
		handle/types.cc
		handle/value_of.cc
		support/callback_info.cc
		support/callback_storage.cc
		support/free_function.cc
		support/initialize.cc
		support/lock.cc
		transfer/accept.cc
		transfer/visit.cc
		value/primitive.cc
		value/value.cc
)

if(NOT EXPORT_IS_EXPORT)
	execute_process(COMMAND node "${CMAKE_CURRENT_SOURCE_DIR}/find.js" OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE ISOLATED_VM_PATH RESULT_VARIABLE RESULT)
	if(NOT RESULT STREQUAL 0)
		message(FATAL_ERROR "Failed to find isolated-vm")
	endif()
	get_filename_component(ISOLATED_VM_SEARCH "${ISOLATED_VM_PATH}" DIRECTORY)
	target_link_directories(isolated_vm PUBLIC "${ISOLATED_VM_SEARCH}")
	target_link_libraries(isolated_vm PRIVATE :backend_napi_v8.node)
endif()
