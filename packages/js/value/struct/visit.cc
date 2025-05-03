module;
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.visit;
import isolated_js.property;
import isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import isolated_js.tag;
import isolated_js.transfer;
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
	public:
		constexpr explicit visit_getter(auto_heritage auto visit_heritage, Getter getter) :
				visit<Meta, typename Getter::type>{visit_heritage},
				getter{std::move(getter)} {}

		constexpr auto operator()(const auto& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, typename Getter::type>& visitor = *this;
			return visitor(getter(value), accept);
		}

	private:
		Getter getter;
};

// Visitor passed to acceptor for each member
template <class Meta, class Property>
struct visit_object_property {
		using first_type = visit_key_literal<Property::property_name, accept_target_t<Meta>>;
		using second_type = visit_getter<Meta, getter_delegate<typename Property::property_type>>;

		constexpr explicit visit_object_property(auto_heritage auto visit_heritage, Property property) :
				second{visit_heritage, getter_delegate{property.value_template}} {}

		constexpr auto make_visit_entry(const auto_accept auto& accept) const {
			return std::pair<typename first_type::visit, const second_type&>{typename first_type::visit{first, accept}, second};
		}

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
	public:
		explicit constexpr visit_struct_properties(auto_heritage auto visit_heritage) :
				properties{std::invoke(
					[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
						return util::make_tuple_in_place(invoke(std::integral_constant<size_t, Index>{})...);
					},
					[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
						using property_type = util::select_t<Index, Property...>;
						return [ & ]() constexpr {
							return visit_object_property<Meta, property_type>{visit_heritage, std::get<Index>(struct_properties<Type>::properties)};
						};
					},
					std::make_index_sequence<sizeof...(Property)>{}
				)} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			auto visit_struct = std::invoke(
				[ & ]<std::size_t... Indices>(const auto& invoke, std::index_sequence<Indices...> /*indices*/) constexpr {
					return util::make_tuple_in_place(invoke(std::integral_constant<size_t, Indices>{})...);
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					return [ & ]() constexpr {
						return std::get<Index>(properties).make_visit_entry(accept);
					};
				},
				std::make_index_sequence<sizeof...(Property)>{}
			);
			return accept(struct_tag<sizeof...(Property)>{}, std::forward<decltype(value)>(value), std::move(visit_struct));
		}

	private:
		std::tuple<visit_object_property<Meta, Property>...> properties;
};

// Visitor function for C++ object types
template <class Meta, class Type>
	requires is_object_struct<Type>
struct visit<Meta, Type> : visit_struct_properties_t<Meta, Type> {
		constexpr explicit visit(auto_heritage auto visit_heritage) :
				visit_struct_properties_t<Meta, Type>{visit_heritage(this)} {}
};

} // namespace js
