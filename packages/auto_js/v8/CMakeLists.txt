find_package(Boost 1.83 REQUIRED)
find_package_from_npm(@auto_js/js)

add_library(v8_js OBJECT)
target_link_libraries(v8_js PUBLIC Boost::boost auto_js google_v8 utility_js)

target_sources(v8_js
	PRIVATE
		evaluation/module.cc
		evaluation/script.cc
		handle/remote.cc
		isolated/agent.cc
		isolated/clock.cc
		isolated/cluster.cc
		isolated/platform.cc
		platform/delegate.cc
		platform/foreground_runner.cc
		platform/worker_runner.cc
		support/error.cc
		support/lock.cc
		value/array_buffer.cc
		value/array.cc
		value/fixed_array.cc
		value/function.cc
		value/object.cc
		value/primitive.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		container/hash.cc
		container/weak_map.cc
		evaluation/module.h.cc
		evaluation/origin.cc
		evaluation/script.h.cc
		handle/collected_handle.cc
		handle/remote.h.cc
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
		value/function.h.cc
		value/handle.cc
		value/object.h.cc
		value/primitive.h.cc
		value/tag.cc
)
