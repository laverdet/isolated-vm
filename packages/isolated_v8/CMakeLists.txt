add_library(isolated_v8 STATIC)
target_sources(isolated_v8
	PRIVATE
		agent/handle.cc
		agent/host.cc
		agent/platform.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		agent/handle.h.cc
		agent/host.h.cc
		agent/platform.h.cc
		cluster.h.cc
)
target_link_libraries(isolated_v8 PRIVATE
	Boost::boost
	google_v8
	ivm_utility
	v8_js
	value_js
)
