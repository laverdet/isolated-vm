module;
#include <cstdint>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.value:struct_.accept;
import :struct_.helpers;
import :tag;
import :visit;

namespace ivm::value {

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
