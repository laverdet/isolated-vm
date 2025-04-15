module;
#include <string_view>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import ivm.utility;

namespace js {

// Check whether or not `object_properties` is defined for a type
export template <class Type>
concept is_object_struct = requires {
	object_properties<Type>::properties;
};

// Remove `void` type identities from tuple
template <class Tuple, class... Types>
struct filter_void_member_types;

template <class... Filtered>
struct filter_void_member_types<std::tuple<Filtered...>>
		: std::type_identity<std::tuple<Filtered...>> {};

template <class... Filtered, class Left, class... Types>
struct filter_void_member_types<std::tuple<Filtered...>, Left, Types...>
		: filter_void_member_types<std::tuple<Filtered..., Left>, Types...> {};

template <class... Filtered, class Left, class... Types>
	requires std::is_void_v<typename Left::type>
struct filter_void_member_types<std::tuple<Filtered...>, Left, Types...>
		: filter_void_member_types<std::tuple<Filtered...>, Types...> {};

// Turns packed types into a tuple, filtering those in which `type` is `void`
export template <class... Types>
using filter_void_members = filter_void_member_types<std::tuple<>, Types...>::type;

// Property name & required flag
template <util::string_literal Name, bool Required>
struct property_info {
		constexpr static auto name = std::string_view{Name};
		constexpr static auto required = Required;
};

// Getter delegate for object property
export template <class Subject, class Property>
struct getter_delegate;

// Getter by free function
template <class Subject, util::string_literal Name, auto Getter, auto Setter, bool Required>
struct getter_delegate<Subject, accessor<Name, Getter, Setter, Required>>
		: std::type_identity<std::decay_t<std::invoke_result_t<decltype(Getter), const Subject&>>>,
			property_info<Name, Required> {
		constexpr auto operator()(const Subject& subject) const -> decltype(auto) { return Getter(subject); }
};

// Getter by member function
template <class S, util::string_literal Name, auto Setter, bool Required, class Type, class Subject, Type (Subject::*Getter)() const>
struct getter_delegate<S, accessor<Name, Getter, Setter, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(const Subject& subject) const -> decltype(auto) { return (subject.*Getter)(); }
};

// Missing getter
template <class Subject, util::string_literal Name, auto Setter>
struct getter_delegate<Subject, accessor<Name, nullptr, Setter, false>> : std::type_identity<void> {};

// Getter by direct member access
template <class S, util::string_literal Name, class Subject, class Type, Type Subject::* Member, bool Required>
struct getter_delegate<S, member<Name, Member, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(const Subject& subject) const -> const Type& { return subject.*Member; }
		// NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
		constexpr auto operator()(Subject&& subject) const -> Type&& { return std::move(subject.*Member); }
};

// Setter delegate for object property
export template <class Subject, class Property>
struct setter_delegate;

// Setter by direct member access
template <class S, util::string_literal Name, class Subject, class Type, Type Subject::* Member, bool Required>
struct setter_delegate<S, member<Name, Member, Required>>
		: std::type_identity<Type>,
			property_info<Name, Required> {
		constexpr auto operator()(Subject& subject, Type value) const -> void { subject.*Member = std::move(value); }
};

// Extract property name from property type
template <class Property>
struct property_name;

export template <class Property>
constexpr auto property_name_v = property_name<Property>::value;

template <class Subject, class Property>
struct property_name<getter_delegate<Subject, Property>> : property_name<Property> {};

template <class Subject, class Property>
struct property_name<setter_delegate<Subject, Property>> : property_name<Property> {};

template <util::string_literal Name, auto Getter, auto Setter, bool Required>
struct property_name<accessor<Name, Getter, Setter, Required>> {
		constexpr static auto value = Name;
};

template <util::string_literal Name, auto Member, bool Required>
struct property_name<member<Name, Member, Required>> {
		constexpr static auto value = Name;
};

} // namespace js
