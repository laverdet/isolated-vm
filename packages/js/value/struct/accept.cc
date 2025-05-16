module;
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.accept;
import isolated_js.property;
import isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Setter delegate for struct property
template <class Value>
struct setter_delegate;

template <class Value>
setter_delegate(Value) -> setter_delegate<Value>;

// Setter by direct member access
template <class Subject, class Type>
struct setter_delegate<struct_member<Subject, Type>> : struct_member<Subject, Type> {
	public:
		explicit constexpr setter_delegate(struct_member<Subject, Type> member) :
				struct_member<Subject, Type>{member} {}

		constexpr auto operator()(Subject& subject, Type value) const -> void {
			subject.*(this->member) = std::move(value);
		}
};

// Property acceptor and setter delegate for one property entry
template <class Meta, class Property>
struct accept_object_property {
	private:
		using member_type = Property::property_type::type;
		using value_type = std::expected<member_type, undefined_in_tag>;
		using accept_type = accept_property_value<Meta, Property::property_name, value_type, typename Meta::visit_subject_type>;
		using setter_type = setter_delegate<typename Property::property_type>;

	public:
		constexpr accept_object_property(auto* previous, Property property) :
				acceptor{previous},
				setter{property.value_template} {}

		constexpr auto operator()(auto&& dictionary, auto& target, const auto& visit) const -> void {
			// nb: We `std::forward` the value to *each* setter. This allows the setters to pick an
			// lvalue object apart member by member if it wants.
			auto value = acceptor(dictionary_tag{}, std::forward<decltype(dictionary)>(dictionary), visit);
			if (value) {
				setter(target, *std::move(value));
			} else if constexpr (!std::is_invocable_v<decltype(setter), std::nullopt_t>) {
				// If the setter accepts undefined values then a missing property is allowed. In this
				// case the setter is not invoked at all, which could in theory be used to distinguish
				// between `undefined` and missing properties.
				const std::string_view name{Property::property_name};
				throw std::logic_error{std::format("Missing required property: {}", name)};
			}
		}

	private:
		[[no_unique_address]] accept_type acceptor;
		[[no_unique_address]] setter_type setter;
};

// Helper to unpack `properties` tuple, construct a new target struct, and invoke each instance of
// `accept_object_property`.
template <class Meta, class Type, class Properties>
struct accept_struct_properties;

template <class Meta, class Type>
using accept_struct_properties_t = accept_struct_properties<Meta, Type, std::decay_t<decltype(struct_properties<Type>::properties)>>;

template <class Meta, class Type, class... Property>
struct accept_struct_properties<Meta, Type, std::tuple<Property...>> {
	public:
		explicit constexpr accept_struct_properties(auto* previous) :
				properties{std::invoke(
					[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
						return util::make_tuple_in_place(invoke(std::integral_constant<size_t, Index>{})...);
					},
					[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
						using property_type = util::select_t<Index, Property...>;
						return [ & ]() constexpr {
							return accept_object_property<Meta, property_type>{previous, std::get<Index>(struct_properties<Type>::properties)};
						};
					},
					std::make_index_sequence<sizeof...(Property)>{}
				)} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			Type target;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& property = std::get<Index>(properties);
					property(std::forward<decltype(dictionary)>(dictionary), target, visit);
				},
				std::make_index_sequence<sizeof...(Property)>{}
			);
			return target;
		}

	private:
		std::tuple<accept_object_property<Meta, Property>...> properties;
};

// Struct acceptor
template <class Meta, class Type>
	requires is_object_struct<Type>
struct accept<Meta, Type> : accept_struct_properties_t<Meta, Type> {
		using accept_struct_properties_t<Meta, Type>::accept_struct_properties_t;
};

} // namespace js
