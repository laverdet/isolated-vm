module;
#include <boost/variant.hpp>
#include <type_traits>
export module ivm.js:variant.types;

namespace js {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant : std::bool_constant<true> {};

export template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

// Helper which holds a recursive `boost::variant` followed by its alternatives
export template <class Variant, class... Types>
struct variant_of;

// Helper which extracts recursive variant alternative types
export template <class First, class... Rest>
using recursive_variant = boost::variant<boost::detail::variant::recursive_flag<First>, Rest...>;

// Helper which substitutes recursive variant alternative types
export template <class Variant, class Type>
using substitute_recursive = typename boost::detail::variant::substitute<Type, Variant, boost::recursive_variant_>::type;

} // namespace js
