add_library(ivm_value OBJECT)
target_sources(ivm_value
	PRIVATE
		assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		dictionary.cc
		enum.cc
		object.cc
		primitive.cc
		tag.cc
		transfer.cc
		tuple.cc
		type/date.cc
		type/dictionary.cc
		type/value.cc
		union.cc
		variant.cc
		visit.cc
)
target_link_libraries(ivm_value PRIVATE
	Boost::boost
	ivm_utility
)
