add_library(napi_js OBJECT)
target_sources(napi_js
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
		support/api.cc
		support/callback_info.cc
		support/callback.cc
		support/environment.cc
		support/finalizer.cc
		support/handle_scope.cc
		support/scheduler.cc
		value/include/array.cc
		value/include/dictionary.cc
		value/include/external.cc
		value/include/function.cc
		value/include/object.cc
		value/include/primitive.cc
		visit.cc
)
target_link_libraries(napi_js PRIVATE
	ivm_utility
	nodejs
	value_js
)
