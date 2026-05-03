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
