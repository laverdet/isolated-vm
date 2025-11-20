module;
#include <optional>
#include <type_traits>
export module auto_js:struct_.types;
import :property;

namespace js {

// You can override this with an object property descriptor for each accepted type
export template <class Type>
struct struct_properties;

// Or, define `properties` on the struct itself
template <class Type>
	requires (Type::struct_template.is_struct_template)
struct struct_properties<Type> {
		constexpr static auto defaultable = std::is_nothrow_constructible_v<Type, std::nullopt_t>;
		constexpr static auto properties = Type::struct_template;
};

// Check whether or not `struct_properties` is defined for a type
template <class Type>
concept transferable_struct = requires { struct_properties<Type>::properties; };

} // namespace js
