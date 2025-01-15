add_library(ivm_utility OBJECT)
target_sources(ivm_utility
	PUBLIC
		FILE_SET CXX_MODULES FILES
			_module.cc
			container/prehashed_string_map.cc
			functional/comparator.cc
			lockable.cc
			ranges.cc
			type_traits.cc
			utility/facade.cc
			utility/mutable_value.cc
			utility/utility.cc
			variant.cc
)
target_link_libraries(ivm_utility PRIVATE Boost::boost)
