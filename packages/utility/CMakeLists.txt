add_library(ivm_utility OBJECT)
target_sources(ivm_utility
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		container/sealed_map.cc
		functional/comparator.cc
		functional/function.cc
		lockable.cc
		memory/autorelease_pool.cc
		memory/comparator.cc
		memory/memory.cc
		ranges.cc
		tuple.cc
		type_traits.cc
		utility/facade.cc
		utility/hash.cc
		utility/string_literal.cc
		utility/utility.cc
		variant.cc
)
# TODO: Only used for libc++ missing features on macOS
target_include_directories(ivm_utility PUBLIC cxx)
target_link_libraries(ivm_utility PRIVATE Boost::boost)
