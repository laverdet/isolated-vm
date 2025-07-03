add_library(ivm_value_napi OBJECT)
target_sources(ivm_value_napi
	PRIVATE
		scheduler.cc
		value/array.cc
		value/dictionary.cc
		value/object.cc
		value/primitive.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		accept.cc
		handle/bound_value.cc
		handle/reference.cc
		handle/remote.cc
		handle/value.cc
		support/callback_info.cc
		support/callback.cc
		support/environment.cc
		support/finalizer.cc
		support/handle_scope.cc
		support/scheduler.cc
		support/utility.cc
		value/include/array.cc
		value/include/dictionary.cc
		value/include/external.cc
		value/include/function.cc
		value/include/object.cc
		value/include/primitive.cc
		visit.cc
)
target_link_libraries(ivm_value_napi PRIVATE
	ivm_utility
	ivm_value
	nodejs
)
