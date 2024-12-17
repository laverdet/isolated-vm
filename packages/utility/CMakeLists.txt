add_library(ivm_utility OBJECT)
target_sources(ivm_utility
	PUBLIC
		FILE_SET CXX_MODULES FILES
			_module.cc
			byte_literals.cc
			collection_group.cc
			comparator.cc
			facade.cc
			lockable.cc
			mutable_value.cc
			prehashed_string_map.cc
			type_traits.cc
			utility.cc
)
target_link_libraries(ivm_utility PRIVATE Boost::boost)
