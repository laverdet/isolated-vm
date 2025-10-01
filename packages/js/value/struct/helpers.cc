export module isolated_js:struct_.helpers;
export import :struct_.types;
import ivm.utility;

namespace js {

// Check whether or not `struct_properties` is defined for a type
export template <class Type>
concept is_object_struct = requires {
	struct_properties<Type>::properties;
};

} // namespace js
