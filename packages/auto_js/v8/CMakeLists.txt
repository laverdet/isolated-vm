find_package(Boost 1.83 REQUIRED)
if(PACKAGE_NAME STREQUAL "@auto_js/v8")
	set(TARGET_NAME "v8_js")
	find_package_from_npm(@auto_js/v8_exports)
elseif(PACKAGE_NAME STREQUAL "@isolated-vm/embedded_v8_js")
	set(TARGET_NAME "embedded_v8_js")
	find_package_from_npm(@auto_js/v8_exports @isolated-vm/embedded_v8)
else()
	message(FATAL_ERROR "Missing or unknown @auto_js/v8 PACKAGE_NAME: '${PACKAGE_NAME}'")
endif()
find_package_from_npm(@auto_js/js)

add_library("${TARGET_NAME}" OBJECT)
target_link_libraries("${TARGET_NAME}" PUBLIC Boost::boost auto_js utility_js)
if(TARGET_NAME STREQUAL "v8_js")
	target_link_libraries("${TARGET_NAME}" PUBLIC nodejs_v8)
else()
	target_compile_definitions("${TARGET_NAME}" PRIVATE INCLUDE_ISOLATED_V8)
	target_link_libraries("${TARGET_NAME}" PUBLIC google_v8)
endif()

target_sources("${TARGET_NAME}"
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
		value/handle.cc
		value/object.h.cc
		value/primitive.h.cc
		value/tag.cc
)

if(TARGET_NAME STREQUAL "embedded_v8_js")
	target_sources("${TARGET_NAME}"
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
