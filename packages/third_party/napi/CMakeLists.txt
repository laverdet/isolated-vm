add_library(ivm_napi STATIC)
target_sources(ivm_napi
	PUBLIC FILE_SET CXX_MODULES FILES
		napi.cc
)

target_include_directories(ivm_napi SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
