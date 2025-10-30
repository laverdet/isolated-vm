add_executable(sandbox)

target_sources(sandbox
	PRIVATE
		sandbox.cc
)
target_link_libraries(sandbox PUBLIC ivm_utility stdc++)
