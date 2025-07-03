add_library(v8_js OBJECT)

target_sources(v8_js
	PRIVATE
		value/array.cc
		value/fixed_array.cc
		value/object.cc
		value/string.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		accept.cc
		container/identity_hash.cc
		container/weak_map.cc
		lock.cc
		value/include/array.cc
		value/include/boolean.cc
		value/include/callback_info.cc
		value/include/date.cc
		value/include/external.cc
		value/include/fixed_array.cc
		value/include/handle.cc
		value/include/number.cc
		value/include/object.cc
		value/include/string.cc
		visit.cc
)
target_link_libraries(v8_js PUBLIC
	google_v8
	ivm_utility
	value_js
)
