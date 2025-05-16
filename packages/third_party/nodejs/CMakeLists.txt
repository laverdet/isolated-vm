add_library(nodejs STATIC)
target_sources(nodejs
	PUBLIC FILE_SET CXX_MODULES FILES
		js_native_api_types.cc
		js_native_api.cc
		node_api.cc
		nodejs.cc
		uv.cc
)

# Required for `napi_create_reference` to non-object types
target_compile_definitions(nodejs PUBLIC NAPI_VERSION=10)

target_include_directories(nodejs SYSTEM PRIVATE ${NODE_DIST_DIR}/include/node)
