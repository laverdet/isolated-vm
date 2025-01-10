add_library(ivm_isolated_v8 STATIC)
target_sources(ivm_isolated_v8
	PRIVATE
		agent.cc
		cluster.cc
		module.cc
		platform/clock.cc
		platform/foreground_runner.cc
		platform/handle.cc
		platform/job_handle.cc
		platform/platform.cc
		realm.cc
		remote.cc
		script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		include/agent_fwd.cc
		include/agent.cc
		include/cluster.cc
		include/module.cc
		include/realm.cc
		include/remote.cc
		include/script.cc
		platform/include/clock.cc
		platform/include/foreground_runner.cc
		platform/include/platform.cc
		platform/include/task_runner.cc
		scheduler.cc
)
target_link_libraries(ivm_isolated_v8 PRIVATE
	Boost::boost
	ivm_embedded_common_v8
	ivm_embedded_v8
	ivm_utility
	ivm_value
)
