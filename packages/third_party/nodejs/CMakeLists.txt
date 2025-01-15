add_library(nodejs STATIC)
target_sources(nodejs
	PUBLIC FILE_SET CXX_MODULES FILES
		js_native_api.cc
		node_api.cc
		nodejs.cc
		uv.cc
)

target_include_directories(nodejs SYSTEM PRIVATE
	${NODE_DIST_DIR}/include/node
)
