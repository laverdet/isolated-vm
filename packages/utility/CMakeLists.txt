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
		memory/noinit_allocator.cc
		ranges.cc
		tuple.cc
		type_traits.cc
		type_traits/traits.cc
		type_traits/type_of.cc
		type_traits/type_pack.cc
		utility/elide.cc
		utility/facade.cc
		utility/hash.cc
		utility/string_literal.cc
		utility/utility.cc
		utility/virtual_covariant.cc
)
target_link_libraries(ivm_utility PRIVATE Boost::boost)
