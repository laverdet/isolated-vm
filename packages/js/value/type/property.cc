module;
#include <optional>
export module isolated_js.property;
import ivm.utility;

namespace js {

// Property flags configuration
export struct property_attributes {
		std::optional<bool> configurable;
		std::optional<bool> enumerable;
		std::optional<bool> writable;
};

// Property descriptor configuration
export template <class Name, class Value>
struct property {
		using property_type = Value;
		constexpr static auto property_name = Name::value;

		consteval property(Name /*name*/, Value value_template) :
				value_template{value_template} {}
		Value value_template;
};

// Value template for struct getter
export template <class Subject, class Type>
struct struct_accessor : std::type_identity<std::decay_t<Type>> {
		explicit constexpr struct_accessor(Type (Subject::*accessor)() const) :
				accessor{accessor} {}

		Type (Subject::*accessor)() const;
};

// Value template for member through direct member access
export template <class Subject, class Type>
struct struct_member : std::type_identity<Type> {
		explicit constexpr struct_member(Type Subject::* member) :
				member{member} {}

		Type Subject::* member;
};

} // namespace js
