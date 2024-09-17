add_library(ivm_value OBJECT)
target_sources(ivm_value
	PRIVATE
		assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		accept.cc
		date.cc
		dictionary.cc
		discriminated_union.cc
		enum.cc
		object.cc
		primitive.cc
		tag.cc
		transfer.cc
		variant.cc
		visit.cc
)
target_link_libraries(ivm_value PRIVATE
	Boost::boost
	ivm_utility
)
