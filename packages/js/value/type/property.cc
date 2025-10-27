module;
#include <cstdint>
#include <tuple>
export module isolated_js:property;
import ivm.utility;

namespace js {

// Properties are either accessors or values
export enum class property_disposition : uint8_t {
	accessor,
	value,
};

// Whether a property is static (on the constructor) or on the prototype (instance).
export enum class class_property_scope : uint8_t {
	constructor,
	prototype,
};

// Property descriptor configuration
export template <auto Name, class Value>
struct property {
		using property_type = Value;
		constexpr static auto property_name = util::cw<Name>;

		consteval property(util::constant_wrapper<Name> /*name*/, Value value_template) :
				value_template{value_template} {}
		Value value_template;
};

// Value template for struct getter
export template <class Subject, class Type>
struct struct_accessor : std::type_identity<std::remove_cvref_t<Type>> {
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

// Holder for class name & constructor
export template <class Name, class Function>
struct class_constructor {
		explicit constexpr class_constructor(Name name, Function function = {}) :
				name{std::move(name)},
				function{std::move(function)} {}

		[[no_unique_address]] Name name;
		[[no_unique_address]] Function function;
};

template <class Name>
class_constructor(Name) -> class_constructor<Name, std::nullptr_t>;

// Class instance functions
// `util::fn` required for member function pointers:
// https://devblogs.microsoft.com/oldnewthing/20040209-00/
// (otherwise they are `> sizeof(void*)` which would break C-style runtime callbacks)
export template <class Name, class Function>
struct class_method {
		explicit constexpr class_method(Name name, Function function) :
				name{std::move(name)},
				function{std::move(function)} {}

		constexpr static auto disposition = property_disposition::value;
		constexpr static auto scope = class_property_scope::prototype;
		[[no_unique_address]] Name name;
		[[no_unique_address]] Function function;
};

// Class static instance function
export template <class Name, class Function>
struct class_static {
		explicit constexpr class_static(Name name, Function function) :
				name{std::move(name)},
				function{std::move(function)} {}

		constexpr static auto disposition = property_disposition::value;
		constexpr static auto scope = class_property_scope::constructor;
		[[no_unique_address]] Name name;
		[[no_unique_address]] Function function;
};

// Class template for runtime host values
export template <class Constructor, class... Properties>
struct class_template {
		explicit constexpr class_template(Constructor constructor, Properties... properties) :
				constructor{std::move(constructor)},
				properties{std::move(properties)...} {}

		[[no_unique_address]] Constructor constructor;
		[[no_unique_address]] std::tuple<Properties...> properties;
};

} // namespace js
