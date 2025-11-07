add_library(v8_js OBJECT)

target_sources(v8_js
	PRIVATE
		handle/remote.cc
		support/lock.cc
		value/array.cc
		value/fixed_array.cc
		value/object.cc
		value/primitive.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		accept.cc
		container/hash.cc
		container/weak_map.cc
		handle/collected_handle.cc
		handle/remote.h.cc
		support/callback_storage.cc
		support/callback.cc
		support/error.cc
		support/lock.h.cc
		support/unmaybe.cc
		value/array.h.cc
		value/callback_info.cc
		value/fixed_array.h.cc
		value/function.cc
		value/handle.cc
		value/object.h.cc
		value/primitive.h.cc
		visit.cc
)
target_link_libraries(v8_js PUBLIC
	google_v8
	ivm_utility
	value_js
)
