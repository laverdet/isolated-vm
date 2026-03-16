find_package_from_npm(@auto_js/utility)

add_library(auto_js OBJECT)
target_compile_features(auto_js PUBLIC cxx_std_23)
target_link_libraries(auto_js PUBLIC utility_js)

target_sources(auto_js
	PRIVATE
		assertions.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		builtin/accept.cc
		builtin/value.cc
		builtin/visit.cc
		enum/accept.cc
		enum/types.cc
		enum/visit.cc
		intrinsics/bigint.cc
		intrinsics/date.cc
		intrinsics/error.cc
		intrinsics/external.cc
		reference/accept.cc
		reference/reference_of.cc
		reference/referential_value.cc
		reference/visit.cc
		struct/accept.cc
		struct/types.cc
		struct/visit.cc
		support/functional.cc
		tag.cc
		transfer/accept.cc
		transfer/deferred_receiver.cc
		transfer/transfer.cc
		transfer/types.cc
		transfer/visit.cc
		tuple/accept.cc
		tuple/visit.cc
		type/function.cc
		type/property.cc
		union/accept.cc
		union/types.cc
		variant/accept.cc
		variant/types.cc
		variant/visit.cc
		vector/accept.cc
		vector/vector_of.cc
		vector/visit.cc
)
