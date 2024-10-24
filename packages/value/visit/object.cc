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

// Getter delegate for object property
template <class Property>
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

// Wrapper for visit result of a struct object
template <class Subject, class... Getters>
struct struct_dictionary {
	private:
		using key_type = std::string;
		using mapped_type = std::variant<typename Getters::type...>;
		using value_type = std::pair<key_type, mapped_type>;

	public:
		constexpr struct_dictionary(const Subject& subject) :
				subject{&subject} {}

		// Iterating over a struct_dictionary (as the target of dictionary_tag) yields a std::variant
		// of all possible property types.
		constexpr auto into_range() {
			auto result =
				make_getters() |
				std::views::transform([ & ](auto getter) -> decltype(auto) { return getter(*subject); });
			return result;
		}

	private:
		consteval static auto make_getters() {
			using variant_getter_type = value_type (*)(const Subject&);
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) consteval {
					return std::array{invoke(std::integral_constant<size_t, Index>{})...};
				},
				[]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr -> variant_getter_type {
					return [](const Subject& subject) constexpr -> value_type {
						using getter_type = std::tuple_element_t<Index, std::tuple<Getters...>>;
						getter_type getter{};
						return std::pair{std::string{getter.name}, mapped_type{std::in_place_index_t<Index>{}, getter(subject)}};
					};
				},
				std::make_index_sequence<sizeof...(Getters)>{}
			);
		}

		const Subject* subject;
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

// Visitor function for C++ object types
template <class Meta, class Type, class... Getters>
struct visit<Meta, object_type<Type, std::tuple<Getters...>>> : visit<Meta, void> {
	private:
		using accept_keys_type = std::tuple<accept_key<Meta, property_name_v<Getters>>...>;
		using visit_values_type = std::tuple<visit<Meta, typename Getters::type>...>;

	public:
		constexpr visit() :
				first{accept_keys},
				second{visit_values} {}
		constexpr visit(int /*dummy*/, const auto_visit auto& /*visit*/) :
				visit{} {}

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(struct_tag<sizeof...(Getters)>{}, value, *this);
		}

	private:
		struct first_type {
				constexpr first_type(const accept_keys_type& accept_keys) :
						accept_keys{accept_keys} {}

				template <size_t Index>
				constexpr auto operator()(
					std::integral_constant<size_t, Index> /*index*/,
					// TODO: This is the subject from `Meta`
					const auto_accept auto& accept
				) const -> decltype(auto) {
					return std::get<Index>(accept_keys)(accept);
				}

				const accept_keys_type& accept_keys;
		};

		struct second_type {
			public:
				constexpr second_type(const visit_values_type& visit_values) :
						visit_values{visit_values} {}

				template <size_t Index>
				constexpr auto operator()(
					std::integral_constant<size_t, Index> /*index*/,
					const auto& dictionary,
					// TODO: This is the subject from `Meta`
					const auto_accept auto& accept
				) const -> decltype(auto) {
					using getter_type = std::tuple_element_t<Index, std::tuple<Getters...>>;
					getter_type getter{};
					return std::get<Index>(visit_values)(getter(dictionary), accept);
				}

			private:
				const visit_values_type& visit_values;
		};

		accept_keys_type accept_keys;
		visit_values_type visit_values;

	public:
		first_type first;
		second_type second;
};

// Apply `getter_delegate` to each property, filtering properties which do not have a getter.
template <class Meta, class Type, class... Properties>
struct visit<Meta, mapped_object_type<Type, std::tuple<Properties...>>>
		: visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Properties>...>>>> {
		using visit<Meta, object_type<Type, remove_void_t<std::tuple<getter_delegate<Properties>...>>>>::visit;
};

// Unpack object properties from `object_properties` specialization
template <class Meta, class Type>
	requires std::destructible<object_properties<Type>>
struct visit<Meta, Type>
		: visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>> {
		using visit<Meta, mapped_object_type<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>::visit;
};

} // namespace ivm::value
