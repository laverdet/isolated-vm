module;
#include <cstdint>
#include <optional>
#include <tuple>
export module isolated_js:property;
import ivm.utility;

namespace js {

// Initializers which are required for `accept<T>` to allow `undefined_tag{}`
export struct optional_constructible {
		optional_constructible() = default;
		explicit optional_constructible(std::nullopt_t /*default*/) noexcept {};
};

// Properties are either accessors or values
export enum class property_disposition : uint8_t {
	accessor,
	function,
	value,
};

// Whether a property is static (on the constructor) or on the prototype (instance).
export enum class class_property_scope : uint8_t {
	constructor,
	prototype,
};

template <class Name>
struct property_name_template {
		explicit constexpr property_name_template(Name name) : name{std::move(name)} {}
		[[no_unique_address]] Name name;
};

template <auto Name>
struct property_name_template<util::constant_wrapper<Name>> {
		explicit constexpr property_name_template(util::constant_wrapper<Name> /*name*/) {}
		constexpr static auto name = util::constant_wrapper<Name>{};
};

template <class Get, class Set>
struct property_accessor_template {
		constexpr property_accessor_template(Get get, Set set) :
				get{std::move(get)},
				set{std::move(set)} {}

		constexpr static auto disposition = property_disposition::accessor;
		[[no_unique_address]] Get get;
		[[no_unique_address]] Set set;
};

template <class Name, class Function>
struct property_function_template : property_name_template<Name> {
		explicit constexpr property_function_template(Name name, Function function) :
				property_name_template<Name>{std::move(name)},
				function{std::move(function)} {}

		constexpr static auto disposition = property_disposition::function;
		[[no_unique_address]] Function function;
};

// Getter delegate for `struct_member`
template <class Subject, class Type>
struct member_getter_delegate {
	public:
		using value_type = Type;
		explicit constexpr member_getter_delegate(Type Subject::* member) : member_{member} {}

		constexpr auto operator()(const Subject& subject) const -> const Type& { return subject.*(this->member_); }
		// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		constexpr auto operator()(Subject&& subject) const -> Type&& { return std::move(subject).*(this->member_); }

	private:
		Type Subject::* member_;
};

// Setter delegate for `struct_member`
template <class Subject, class Type>
struct member_setter_delegate {
	public:
		using value_type = Type;
		explicit constexpr member_setter_delegate(Type Subject::* member) : member_{member} {}

		constexpr auto operator()(Subject& subject, const Type& value) const -> void { subject.*(this->member_) = value; }
		constexpr auto operator()(Subject& subject, Type&& value) const -> void { subject.*(this->member_) = std::move(value); }

	private:
		Type Subject::* member_;
};

// Property template via direct member access
export template <class Name, class Subject, class Type>
struct struct_member
		: property_name_template<Name>,
			property_accessor_template<member_getter_delegate<Subject, Type>, member_setter_delegate<Subject, Type>> {
		constexpr struct_member(Name name, Type Subject::* member) :
				property_name_template<Name>{std::move(name)},
				property_accessor_template<member_getter_delegate<Subject, Type>, member_setter_delegate<Subject, Type>>{
					member_getter_delegate<Subject, Type>{member},
					member_setter_delegate<Subject, Type>{member}
				} {}
};

// Getter delegate for `struct_accessor`
template <class Get>
struct accessor_getter_delegate;

template <class Signature>
struct accessor_getter_signature;

template <class Get>
struct accessor_getter_delegate : Get, accessor_getter_signature<util::function_signature_t<Get>> {
		// nb: This would fail for plain function pointers
		constexpr explicit accessor_getter_delegate(Get get) : Get{std::move(get)} {}
};

template <class Subject, class Type, bool Nx>
struct accessor_getter_signature<auto(const Subject&) noexcept(Nx)->Type> {
		using value_type = std::remove_cvref_t<Type>;
};

// Setter delegate for `struct_accessor`
template <class Set>
struct accessor_setter_delegate;

template <class Signature>
struct accessor_setter_signature;

template <class Set>
struct accessor_setter_delegate : Set, accessor_setter_signature<util::function_signature_t<Set>> {
		// nb: This would fail for plain function pointers
		constexpr explicit accessor_setter_delegate(Set set) : Set{std::move(set)} {}
};

template <>
struct accessor_setter_delegate<std::nullptr_t> {
		constexpr explicit accessor_setter_delegate(std::nullptr_t /*set*/) {}
};

template <class Subject, class Type, bool Nx>
struct accessor_setter_signature<auto(Subject&, Type) noexcept(Nx)->void> {
		using value_type = std::remove_cvref_t<Type>;
};

// Property template through getter & setter
export template <class Name, class Get, class Set>
struct struct_accessor
		: property_name_template<Name>,
			property_accessor_template<accessor_getter_delegate<Get>, accessor_setter_delegate<Set>> {
		constexpr struct_accessor(Name name, Get get, Set set = {}) :
				property_name_template<Name>{std::move(name)},
				property_accessor_template<accessor_getter_delegate<Get>, accessor_setter_delegate<Set>>{
					accessor_getter_delegate<Get>{std::move(get)},
					accessor_setter_delegate<Set>{std::move(set)}
				} {}
};

template <class Name, class Get>
struct_accessor(Name, Get) -> struct_accessor<Name, Get, std::nullptr_t>;

// Struct template holder
export template <class... Properties>
struct struct_template : std::tuple<Properties...> {
		explicit constexpr struct_template(Properties... properties) :
				std::tuple<Properties...>{std::move(properties)...} {}
		constexpr static auto is_struct_template = true;
};

// Holder for class name & constructor
export template <class Name, class Function>
struct class_constructor : property_function_template<Name, Function> {
		explicit constexpr class_constructor(Name name, Function function = {}) :
				property_function_template<Name, Function>{std::move(name), std::move(function)} {}
};

template <class Name>
class_constructor(Name) -> class_constructor<Name, std::nullptr_t>;

// Class instance functions
// `util::fn` required for member function pointers:
// https://devblogs.microsoft.com/oldnewthing/20040209-00/
// (otherwise they are `> sizeof(void*)` which would break C-style runtime callbacks)
export template <class Name, class Function>
struct class_method : property_function_template<Name, Function> {
		using property_function_template<Name, Function>::property_function_template;
		constexpr static auto scope = class_property_scope::prototype;
};

template <class Name, class Function>
class_method(Name, Function) -> class_method<Name, Function>;

// Class static instance function
export template <class Name, class Function>
struct class_static : property_function_template<Name, Function> {
		using property_function_template<Name, Function>::property_function_template;
		constexpr static auto scope = class_property_scope::constructor;
};

template <class Name, class Function>
class_static(Name, Function) -> class_static<Name, Function>;

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
