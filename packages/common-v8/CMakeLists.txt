add_library(${IV8_PREFIX}_common_v8 OBJECT)
target_sources(${IV8_PREFIX}_common_v8
	PRIVATE
		value/array.cc
		value/object.cc
		value/string.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		accept.cc
		include/_module.cc
		include/array.cc
		include/boolean.cc
		include/date.cc
		include/external.cc
		include/handle.cc
		include/number.cc
		include/object.cc
		include/string.cc
		utility.cc
		visit.cc
)
target_link_libraries(${IV8_PREFIX}_common_v8 PUBLIC
	${IV8_PREFIX}_v8
	ivm_utility
	ivm_value
)
