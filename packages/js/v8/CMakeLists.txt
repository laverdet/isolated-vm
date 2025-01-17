add_library(${IV8_PREFIX}_common_v8 OBJECT)
target_sources(${IV8_PREFIX}_common_v8
	PRIVATE
		value/array.cc
		value/fixed_array.cc
		value/object.cc
		value/string.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		accept.cc
		container/weak_map.cc
		lock.cc
		value/include/array.cc
		value/include/boolean.cc
		value/include/date.cc
		value/include/external.cc
		value/include/fixed_array.cc
		value/include/handle.cc
		value/include/number.cc
		value/include/object.cc
		value/include/string.cc
		visit.cc
)
target_link_libraries(${IV8_PREFIX}_common_v8 PUBLIC
	${IV8_PREFIX}_v8
	ivm_utility
	ivm_value
)
