add_library(ivm_value OBJECT)
target_sources(ivm_value
	PRIVATE
		assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		dictionary/accept.cc
		dictionary/helpers.cc
		dictionary/vector_of.cc
		dictionary/visit.cc
		enum/accept.cc
		enum/types.cc
		primitive/accept.cc
		primitive/tag.cc
		primitive/types.cc
		primitive/visit.cc
		struct/accept.cc
		struct/helpers.cc
		struct/types.cc
		struct/visit.cc
		tag.cc
		transfer.cc
		tuple/accept.cc
		tuple/visit.cc
		type/date.cc
		type/value.cc
		union/accept.cc
		union/types.cc
		variant/accept.cc
		variant/types.cc
		variant/visit.cc
		vector/visit.cc
		visit.cc
)
target_link_libraries(ivm_value PRIVATE
	Boost::boost
	ivm_utility
)
