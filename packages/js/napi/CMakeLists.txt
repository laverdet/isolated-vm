add_library(ivm_value_napi OBJECT)
target_sources(ivm_value_napi
	PRIVATE
		value/array.cc
		value/handle.cc
		value/number.cc
		value/object_like.cc
		value/object.cc
		value/string.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		accept.cc
		include/_module.cc
		include/array.cc
		include/handle.cc
		include/number.cc
		include/object_like.cc
		include/object.cc
		include/string.cc
		include/utility.cc
		include/value_of.cc
		visit.cc
)
target_compile_definitions(ivm_value_napi PRIVATE
	NAPI_VERSION=9
)
target_link_libraries(ivm_value_napi PRIVATE
	ivm_napi
	ivm_nodejs_common_v8
	ivm_utility
)
