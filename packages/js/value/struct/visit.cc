module;
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js:struct_.visit;
import :property;
import :struct_.helpers;
import :transfer;
import ivm.utility;

namespace js {

// Getter delegate for struct property
template <class Value>
struct getter_delegate;

template <class Value>
getter_delegate(Value) -> getter_delegate<Value>;

// Getter by accessor delegate
template <class Subject, class Type>
struct getter_delegate<struct_accessor<Subject, Type>> : struct_accessor<Subject, Type> {
		explicit constexpr getter_delegate(struct_accessor<Subject, Type> accessor) :
				struct_accessor<Subject, Type>{accessor} {}

		constexpr auto operator()(const Subject& subject) const -> decltype(auto) { return (subject.*this->accessor)(); }
};

// Getter by direct member access
template <class Subject, class Type>
struct getter_delegate<struct_member<Subject, Type>> : struct_member<Subject, Type> {
		explicit constexpr getter_delegate(struct_member<Subject, Type> member) :
				struct_member<Subject, Type>{member} {}

		constexpr auto operator()(const Subject& subject) const -> const Type& { return subject.*(this->member); }
		constexpr auto operator()(Subject&& subject) const -> Type&& { return std::move(subject).*(this->member); }
};

// Visit a member of a struct via a getter delegate. This gets passed along to whatever acceptor
// takes the `struct_tag`.
template <class Meta, class Getter>
struct visit_getter : visit<Meta, typename Getter::type> {
		using visit_type = visit<Meta, typename Getter::type>;
		constexpr visit_getter(auto* transfer, Getter getter) :
				visit_type{transfer},
				getter{std::move(getter)} {}

		constexpr auto operator()(const auto& value, auto& accept) const -> decltype(auto) {
			return visit_type::operator()(getter(value), accept);
		}

	private:
		Getter getter;
};

// Visitor passed to acceptor for each member
template <class Meta, class Property>
struct visit_object_property {
		using first_type = visit_key_literal<Property::property_name, typename Meta::accept_target_type>;
		using second_type = visit_getter<Meta, getter_delegate<typename Property::property_type>>;

		constexpr visit_object_property(auto* transfer, Property property) :
				second{transfer, getter_delegate{property.value_template}} {}

		[[no_unique_address]] first_type first;
		[[no_unique_address]] second_type second;
};

// Helper to unpack `properties` tuple, construct individual property visitors, and forward struct
// to acceptors & getters.
template <class Meta, class Type, class Properties>
struct visit_struct_properties;

template <class Meta, class Type>
using visit_struct_properties_t = visit_struct_properties<Meta, Type, std::decay_t<decltype(struct_properties<Type>::properties)>>;

template <class Meta, class Type, class... Property>
struct visit_struct_properties<Meta, Type, std::tuple<Property...>> {
	private:
		using properties_type = std::tuple<visit_object_property<Meta, Property>...>;

	public:
		explicit constexpr visit_struct_properties(auto* transfer) :
				properties{std::invoke(
					[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
						return properties_type{util::elide{invoke, std::integral_constant<size_t, Index>{}}...};
					},
					[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
						using property_type = Property...[ Index ];
						return visit_object_property<Meta, property_type>{transfer, std::get<Index>(struct_properties<Type>::properties)};
					},
					std::make_index_sequence<sizeof...(Property)>{}
				)} {}

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, struct_tag<sizeof...(Property)>{}, properties, std::forward<decltype(value)>(value));
		}

	private:
		properties_type properties;
};

// Visitor function for C++ object types
template <class Meta, class Type>
	requires is_object_struct<Type>
struct visit<Meta, Type> : visit_struct_properties_t<Meta, Type> {
		using visit_struct_properties_t<Meta, Type>::visit_struct_properties_t;
};

} // namespace js
