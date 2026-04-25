add_library(isolated_vm OBJECT)

target_sources(isolated_vm
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		initialize.h.cc
)
