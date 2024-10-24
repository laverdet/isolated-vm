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
		scheduler.cc
		script.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		include/_module.cc
		include/agent_fwd.cc
		include/agent.cc
		include/cluster.cc
		include/module.cc
		include/platform.cc
		include/platform/clock.cc
		include/platform/foreground_runner.cc
		include/platform/task_runner.cc
		include/realm.cc
		include/scheduler.cc
		include/script.cc
)
target_link_libraries(ivm_isolated_v8 PRIVATE
	Boost::boost
	ivm_embedded_common_v8
	ivm_embedded_v8
	ivm_utility
	ivm_value
)
