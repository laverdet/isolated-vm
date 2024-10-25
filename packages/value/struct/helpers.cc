module;
#include <string_view>
#include <type_traits>
#include <utility>
export module ivm.value:struct_.helpers;
import :struct_.types;
import ivm.utility;

namespace ivm::value {

// Helper for object accept & visit
export template <class Type, class Properties>
struct object_type;

export template <class Type, class Properties>
struct mapped_object_type;

// Remove `void` type identities from tuple
template <class Left, class>
struct remove_void_helper : std::type_identity<Left> {};

template <class Type, class... Left, class... Right>
struct remove_void_helper<std::tuple<Left...>, std::tuple<Type, Right...>>
		: remove_void_helper<
				std::conditional_t<std::is_void_v<typename Type::type>, std::tuple<Left...>, std::tuple<Left..., Type>>,
				std::tuple<Right...>> {};

export template <class Type>
using remove_void_t = remove_void_helper<std::tuple<>, Type>::type;

// Property name & required flag
template <util::string_literal Name, bool Required>
struct property_info {
		constexpr static auto name = std::string_view{Name};
		constexpr static auto required = Required;
};

// Getter delegate for object property
export template <class Property>
struct getter_delegate;

template <util::string_literal Name, bool Required, auto Setter, class Type, class Subject, Type (Subject::* Getter)()>
struct getter_delegate<accessor<Name, Getter, Setter, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(Subject& subject) const -> Type { return (&subject->*Getter)(); }
};

template <util::string_literal Name, auto Setter>
struct getter_delegate<accessor<Name, nullptr, Setter, false>> : std::type_identity<void> {};

template <util::string_literal Name, bool Required, class Subject, class Type, Type Subject::* Member>
struct getter_delegate<member<Name, Member, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(const Subject& subject) const -> const Type& { return subject.*Member; }
};

// Setter delegate for object property
export template <class Property>
struct setter_delegate;

template <util::string_literal Name, bool Required, auto Getter, class Type, class Subject, void (Subject::* Setter)(Type)>
struct setter_delegate<accessor<Name, Getter, Setter, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(Subject& subject, Type value) const -> void { (&subject->*Setter)(std::forward<Type>(value)); }
};

template <util::string_literal Name, auto Getter>
struct setter_delegate<accessor<Name, Getter, nullptr, false>> : std::type_identity<void> {};

template <util::string_literal Name, bool Required, class Subject, class Type, Type Subject::* Member>
struct setter_delegate<member<Name, Member, Required>>
		: std::type_identity<std::decay_t<Type>>,
			property_info<Name, Required> {
		constexpr auto operator()(Subject& subject, const Type& value) const -> void { subject.*Member = std::move(value); }
};

// Extract property name from property type
template <class Property>
struct property_name;

export template <class Property>
constexpr auto property_name_v = property_name<Property>::value;

template <class Type>
struct property_name<getter_delegate<Type>> : property_name<Type> {};

template <class Type>
struct property_name<setter_delegate<Type>> : property_name<Type> {};

template <util::string_literal Name, auto Getter, auto Setter, bool Required>
struct property_name<accessor<Name, Getter, Setter, Required>> {
		constexpr static auto value = Name;
};

template <util::string_literal Name, auto Member, bool Required>
struct property_name<member<Name, Member, Required>> {
		constexpr static auto value = Name;
};

} // namespace ivm::value
