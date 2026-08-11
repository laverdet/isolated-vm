export module auto_js:variant.types;
import :referential_value;
import std;
import util;

namespace js {

// Generalized variant discriminators
export template <class Type>
struct variant_discriminator;

template <class Type>
	requires(Type::variant.is_variant_discriminator)
struct variant_discriminator<Type> {
		constexpr static auto value = Type::variant;
};

// Extract `std::variant` types for `reference_value`
constexpr auto variant_types_from =
	[]<class... Types>(std::type_identity<std::variant<Types...>> /*type*/) constexpr { return util::type_pack{type<Types>...}; };

// Instantiate a `referential_value` type which holds a `std::variant` with circular reference
// storage.
export template <template <class> class Make>
using referential_variant = recursive_value_holder<Make, variant_types_from>;

} // namespace js
