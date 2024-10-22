module;
#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
export module ivm.value:object;
import ivm.utility;
import :primitive;
import :tag;
import :visit;

namespace ivm::value {

// You override this with an object property descriptor for each accepted type
export template <class Type>
struct object_properties;

// Descriptor for object getter and/or setter
export template <util::string_literal Name, auto Getter, auto Setter, bool Required = true>
struct accessor {};

// Descriptor for object property through direct member access
export template <util::string_literal Name, auto Member, bool Required = true>
struct member {};

// Remove `void` type identities from tuple
template <class Left, class>
struct remove_void_helper : std::type_identity<Left> {};

template <class Type, class... Left, class... Right>
struct remove_void_helper<std::tuple<Left...>, std::tuple<Type, Right...>>
		: remove_void_helper<
				std::conditional_t<std::is_void_v<typename Type::type>, std::tuple<Left...>, std::tuple<Left..., Type>>,
				std::tuple<Right...>> {};

template <class Type>
using remove_void_t = remove_void_helper<std::tuple<>, Type>::type;

// Property name & required flag
template <util::string_literal Name, bool Required>
struct property_info {
		constexpr static auto name = std::string_view{Name};
		constexpr static auto required = Required;
};

// Setter delegate for object property
template <class Property>
struct setter_delegate;

template <class Property>
using setter_delegate_t = setter_delegate<Property>::type;

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

template <class Property>
constexpr auto property_name_v = property_name<Property>::value;

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

// Helper for object accept & visit
template <class Type, class Properties>
struct object_type;

template <class Type, class Properties>
struct mapped_object_type;

// Acceptor function for C++ object types.
template <class Meta, class Type, class... Setters>
struct accept<Meta, object_type<Type, std::tuple<Setters...>>> : accept<Meta, void> {
	public:
		using hash_type = uint32_t;

		constexpr accept(const auto_visit auto& visit) :
				accept<Meta, void>{visit},
				first{visit},
				second{accept_next<Meta, typename Setters::type>{visit}...},
				visit_properties{visit_key<Meta, property_name_v<Setters>>{visit}...} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*acceptor*/) :
				accept{visit} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			Type subject;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					using setter_type = std::tuple_element_t<Index, std::tuple<Setters...>>;
					setter_type setter{};
					const auto& accept = std::get<Index>(second);
					const auto& visit_property = std::get<Index>(visit_properties);
					auto value = visit_property(dictionary, visit, accept);
					if (value) {
						setter(subject, *std::move(value));
					} else if (setter.required) {
						throw std::logic_error(std::format("Missing required property: {}", setter.name));
					}
				},
				std::make_index_sequence<sizeof...(Setters)>{}
			);
			return subject;
		}

	private:
		accept_next<Meta, std::string> first;
		std::tuple<accept_next<Meta, typename Setters::type>...> second;
		std::tuple<visit_key<Meta, property_name_v<Setters>>...> visit_properties;
};

// Apply `setter_delegate` to each property, filtering properties which do not have a setter.
template <class Meta, class Type, class... Properties>
struct accept<Meta, mapped_object_type<Type, std::tuple<Properties...>>>
		: accept<Meta, object_type<Type, remove_void_t<std::tuple<setter_delegate<Properties>...>>>> {
		using accept<Meta, object_type<Type, remove_void_t<std::tuple<setter_delegate<Properties>...>>>>::accept;
};

// Unpack object properties from `object_properties` specialization
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct accept<Meta, Type> : accept<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>> {
		using accept<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>::accept;
};

} // namespace ivm::value
