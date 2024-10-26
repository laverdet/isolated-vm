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
	private:
		template <class Setter>
		using accept_setter = accept<Meta, value_by_key<property_name_v<Setter>, std::optional<typename Setter::type>, visit_subject_t<Meta>>>;

	public:
		using hash_type = uint32_t;

		constexpr accept(const auto_visit auto& visit) :
				accept<Meta, void>{visit},
				first{visit},
				second{accept_setter<Setters>{visit}...} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept_*/) :
				accept{visit} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> Type {
			Type subject;
			std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					(invoke(std::integral_constant<size_t, Index>{}), ...);
				},
				[ & ]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					util::select_t<Index, Setters...> setter{};
					auto value = std::get<Index>(second)(dictionary_tag{}, dictionary, visit);
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
		std::tuple<accept_setter<Setters>...> second;
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
