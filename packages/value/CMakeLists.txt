add_library(ivm_value OBJECT)
target_sources(ivm_value
	PRIVATE
		assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		tag.cc
		transfer.cc
		type/date.cc
		type/dictionary.cc
		type/value.cc
		visit.cc
		visit/dictionary.cc
		visit/enum.cc
		visit/object.cc
		visit/primitive.cc
		visit/tuple.cc
		visit/union.cc
		visit/variant.cc
		visit/vector.cc
)
target_link_libraries(ivm_value PRIVATE
	Boost::boost
	ivm_utility
)
