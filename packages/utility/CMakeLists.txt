add_library(utility_js OBJECT)
target_compile_features(utility_js PUBLIC cxx_std_23)

# Setup `#include "auto_js/[..]"`
target_include_directories(utility_js PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(utility_js
	PRIVATE
		assertions.cc
		platform/timer.cc
	PUBLIC FILE_SET CXX_MODULES FILES
		_module.cc
		container/mutable_priority_queue.cc
		container/sealed_map.cc
		container/segmented_priority_queue.cc
		container/tinker_queue.cc
		functional/bind.cc
		functional/elide.cc
		functional/flat_tuple.cc
		functional/function_constant.cc
		functional/function_ref.cc
		functional/functional.cc
		functional/regular_return.cc
		memory/autorelease_pool.cc
		memory/comparator.cc
		memory/memory.cc
		memory/noinit_allocator.cc
		meta/algorithm.cc
		platform/lockable.cc
		platform/stop_token.cc
		platform/timer.h.cc
		type_traits/function_traits.cc
		type_traits/transform.cc
		type_traits/type_of.cc
		type_traits/type_pack.cc
		type_traits/type_traits.cc
		utility/constant_wrapper.cc
		utility/covariant_value.cc
		utility/facade.cc
		utility/hash.cc
		utility/ranges.cc
		utility/string.cc
		utility/utility.cc
		utility/variant.cc
)
