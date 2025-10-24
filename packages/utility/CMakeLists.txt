add_library(ivm_utility OBJECT)
target_sources(ivm_utility
	PRIVATE
		timer.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		container/sealed_map.cc
		functional/function.cc
		lockable.cc
		memory/autorelease_pool.cc
		memory/comparator.cc
		memory/memory.cc
		memory/noinit_allocator.cc
		ranges.cc
		timer_.cc
		tuple.cc
		type_traits/transform.cc
		type_traits/type_of.cc
		type_traits/type_pack.cc
		type_traits/type_traits.cc
		utility/constant_wrapper.cc
		utility/elide.cc
		utility/facade.cc
		utility/function_ref.cc
		utility/hash.cc
		utility/utility.cc
		utility/virtual_covariant.cc
)
target_link_libraries(ivm_utility PRIVATE Boost::boost)
