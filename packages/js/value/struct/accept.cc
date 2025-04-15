module;
#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.struct_.accept;
import isolated_js.struct_.helpers;
import isolated_js.struct_.types;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Acceptor function for C++ object types.
template <class Meta, class Type, class Setters>
struct accept_object_target;

template <class Meta, class Type, class... Setters>
struct accept_object_target<Meta, Type, std::tuple<Setters...>> {
	private:
		template <class Setter>
		using setter_helper = accept_property_value<Meta, property_name_v<Setter>, std::optional<typename Setter::type>, visit_subject_t<Meta>>;

	public:
		explicit constexpr accept_object_target(auto_heritage auto accept_heritage) :
				first{accept_heritage},
				second{util::make_tuple_in_place(
					[ & ] constexpr { return setter_helper<Setters>{accept_heritage}; }...
				)} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			Type subject;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					util::select_t<Index, Setters...> setter{};
					// nb: We `std::forward` the value to *each* setter. This allows the setters to pick an
					// lvalue object apart member by member if it wants.
					auto& accept = std::get<Index>(second);
					auto value = accept(dictionary_tag{}, std::forward<decltype(dictionary)>(dictionary), visit);
					if (value) {
						setter(subject, *std::move(value));
					} else if (setter.required) {
						throw std::logic_error{std::format("Missing required property: {}", setter.name)};
					}
				},
				std::make_index_sequence<sizeof...(Setters)>{}
			);
			return subject;
		}

	private:
		accept_next<Meta, std::string> first;
		std::tuple<setter_helper<Setters>...> second;
};

// Helper to unpack `properties` tuple, apply setter delegate, filter void members, and repack for
// `accept_object_target`
template <class Type, class Properties>
struct expand_object_setters;

template <class Type, class Properties>
using expand_object_setters_t = expand_object_setters<Type, Properties>::type;

template <class Type, class... Properties>
struct expand_object_setters<Type, std::tuple<Properties...>>
		: std::type_identity<filter_void_members<setter_delegate<Type, Properties>...>> {};

// Forward object property tuple from `object_properties` specialization
template <class Meta, class Type>
using accept_object_target_type =
	accept_object_target<Meta, Type, expand_object_setters_t<Type, std::decay_t<decltype(object_properties<Type>::properties)>>>;

template <class Meta, class Type>
	requires is_object_struct<Type>
struct accept<Meta, Type>
		: accept_object_target_type<Meta, Type> {
		using accept_object_target_type<Meta, Type>::accept_object_target_type;
};

} // namespace js
