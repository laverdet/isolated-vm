add_library(ivm_isolated_v8 STATIC)
target_sources(ivm_isolated_v8
	PRIVATE
		agent.cc
		cluster.cc
		evaluation/module.cc
		evaluation/script.cc
		platform/clock.cc
		platform/foreground_runner.cc
		platform/handle.cc
		platform/job_handle.cc
		platform/platform.cc
		realm.cc
		remote.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		evaluation/include/module.cc
		evaluation/include/script.cc
		include/agent_fwd.cc
		include/agent.cc
		include/cluster.cc
		include/realm.cc
		include/remote.cc
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
