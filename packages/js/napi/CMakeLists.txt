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
		handle/internal.cc
		handle/reference.cc
		handle/remote.cc
		handle/value.cc
		support/environment.cc
		support/include/handle_scope.cc
		support/include/lock.cc
		support/include/scheduler.cc
		utility.cc
		value/include/array.cc
		value/include/dictionary.cc
		value/include/function.cc
		value/include/object.cc
		value/include/primitive.cc
		visit.cc
)
target_compile_definitions(ivm_value_napi PRIVATE
	NAPI_VERSION=9
)
target_include_directories(ivm_value_napi SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
target_link_libraries(ivm_value_napi PRIVATE
	backend_napi_v8_common_v8
	ivm_utility
	nodejs
)
