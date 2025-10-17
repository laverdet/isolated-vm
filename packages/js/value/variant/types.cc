module;
#include <type_traits>
#include <variant>
export module isolated_js:variant.types;
import :referential_value;
import ivm.utility;

namespace js {

// Specialize in order to disable `std::variant` visitor
template <class... Types>
struct is_variant : std::bool_constant<true> {};

template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

// Extract `std::variant` types for `reference_value`
constexpr auto variant_types_from =
	[]<class... Types>(std::type_identity<std::variant<Types...>> /*type*/) constexpr
	-> util::type_pack<Types...> {
	return {};
};

// Instantiate a `referential_value` type which holds a `std::variant` with circular reference
// storage.
export template <template <class> class Make>
using referential_variant = referential_value<Make, variant_types_from>;

} // namespace js
