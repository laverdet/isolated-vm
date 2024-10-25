module;
#include <boost/variant.hpp>
#include <type_traits>
export module ivm.value:dictionary.types;

namespace ivm::value {

// Look for `boost::recursive_variant_` to determine if this container is recursive
export template <class Type>
struct is_recursive : std::bool_constant<false> {};

template <class Type>
constexpr auto is_recursive_v = is_recursive<Type>::value;

template <>
struct is_recursive<boost::recursive_variant_> : std::bool_constant<true> {};

template <template <class...> class Type, class... Types>
struct is_recursive<Type<Types...>>
		: std::bool_constant<std::disjunction_v<is_recursive<Types>...>> {};

// Helper to instantiate a different `accept` or `visit` structure for recursive containers
export template <class Type>
struct dictionary_subject;

} // namespace ivm::value
