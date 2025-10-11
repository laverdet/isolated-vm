module;
#include <type_traits>
export module isolated_js:variant.types;
import :recursive_value;
import ivm.utility;

namespace js {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant : std::bool_constant<true> {};

export template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

} // namespace js
