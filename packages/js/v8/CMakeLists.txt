add_library(${IV8_PREFIX}_common_v8 OBJECT)

# Dual module build for embedded / nodejs v8
if (IVM_IV8_TRICKSHOT)
	set(V8_JS_MODULE_NAME z8_js)
	target_compile_definitions(${IV8_PREFIX}_common_v8 PUBLIC
		iv8=iz8
		v8_js=z8_js
	)
else()
	set(V8_JS_MODULE_NAME v8_js)
endif()

target_preprocessed_sources(${IV8_PREFIX}_common_v8 v8_js ${V8_JS_MODULE_NAME}
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
target_link_libraries(${IV8_PREFIX}_common_v8 PUBLIC
	${IV8_PREFIX}_v8
	ivm_utility
	ivm_value
)
