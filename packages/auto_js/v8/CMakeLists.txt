# Locate dependencies
find_package(Boost 1.83 REQUIRED)
find_package_from_npm(@auto_js/v8_exports AS @auto_js/${AUTO_JS_PREFIX}v8_exports PROPAGATE google_v8)
find_package_from_npm(@auto_js/js AS @auto_js/${AUTO_JS_PREFIX}js PROPAGATE auto_js)

# Initialize target [v8_js or v8_isolated_js]
set(v8_js ${AUTO_JS_PREFIX}v8_js)
add_library(${v8_js} STATIC)
set_npm_package_properties(@auto_js/v8 auto_js v8_js)
target_compile_features(${auto_js} PUBLIC cxx_std_23)

# Link dependencies
target_link_libraries(${v8_js} PRIVATE Boost::boost ${auto_js} utility_js)
target_link_libraries(${v8_js} PUBLIC ${google_v8})

# Common sources (nodejs + iv8::isolated)
target_sources(${v8_js}
	PRIVATE
		handle/remote.cc
		support/error.cc
		support/lock.cc
		transfer/accept.cc
		value/array_buffer.cc
		value/array.cc
		value/fixed_array.cc
		value/object.cc
		value/primitive.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		container/hash.cc
		container/weak_map.cc
		handle/collected_handle.cc
		handle/remote.h.cc
		handle/value_of.cc
		support/boost.h.cc
		support/callback_storage.cc
		support/callback.cc
		support/error.h.cc
		support/lock.h.cc
		support/unmaybe.cc
		transfer/accept.h.cc
		transfer/visit.cc
		value/array_buffer.h.cc
		value/array.h.cc
		value/callback_info.cc
		value/fixed_array.h.cc
		value/function.cc
		value/function.h.cc
		value/handle.cc
		value/object.h.cc
		value/primitive.h.cc
		value/tag.cc
		value/value.cc
)

# iv8::isolated sources
if(v8_js STREQUAL isolated_v8_js)
	target_compile_definitions(${v8_js} PRIVATE INCLUDE_ISOLATED_V8)
	target_sources(${v8_js}
		PRIVATE
			evaluation/module.cc
			evaluation/script.cc
			isolated/agent.cc
			isolated/clock.cc
			isolated/cluster.cc
			isolated/platform.cc
			platform/delegate.cc
			platform/foreground_runner.cc
			platform/worker_runner.cc
		PUBLIC FILE_SET CXX_MODULES FILES
			evaluation/module.h.cc
			evaluation/origin.cc
			evaluation/script.h.cc
			isolated/agent.h.cc
			isolated/clock.h.cc
			isolated/cluster.h.cc
			isolated/platform.h.cc
			platform/allocator.h.cc
			platform/delegate.h.cc
			platform/foreground_runner.h.cc
			platform/scheduler.cc
			platform/task_queue.cc
			platform/task_runner.cc
			platform/task.cc
			platform/worker_runner.h.cc
	)
endif()
