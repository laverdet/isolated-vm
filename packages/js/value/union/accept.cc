module;
#include <concepts>
#include <format>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js.union_.accept;
import isolated_js.tag;
import isolated_js.transfer;
import isolated_js.union_.types;
import isolated_js.variant.types;
import ivm.utility;

namespace js {

// Helper concept
template <class Type>
concept is_discriminated_union_descriptor = requires {
	Type::alternatives;
	Type::discriminant;
};

template <class... Types>
concept is_discriminated_union =
	is_discriminated_union_descriptor<union_of<std::variant<Types...>>>;

// Suppress `std::variant` visitor
template <class... Types>
	requires is_discriminated_union<Types...>
struct is_variant<Types...> : std::bool_constant<false> {};

// Acceptor for discriminated union/variant types
template <class Meta, class... Types>
	requires is_discriminated_union<Types...>
struct accept<Meta, std::variant<Types...>> {
	public:
		using accepted_type = std::variant<Types...>;
		using descriptor_type = union_of<accepted_type>;

		explicit constexpr accept(auto_heritage auto accept_heritage) :
				second{util::make_tuple_in_place(
					[ & ] constexpr { return accept_next<Meta, Types>{accept_heritage}; }...
				)},
				accept_discriminant{accept_heritage} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(dictionary), decltype(visit)>();
			auto discriminant_value = accept_discriminant(dictionary_tag{}, dictionary, visit);
			auto accept_alternative = alternatives.get(discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error{std::format("Unknown discriminant: {}", discriminant_value)};
			}
			return (*accept_alternative)(*this, std::forward<decltype(dictionary)>(dictionary), visit);
		}

	private:
		template <class Value, class Visit>
		consteval static auto make_discriminant_map() {
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) consteval {
					return util::prehashed_string_map{invoke(std::integral_constant<size_t, Index>{})...};
				},
				[]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& alternative = std::get<Index>(descriptor_type::alternatives);
					return std::pair{alternative.discriminant, &accept_value<Index, Value, Visit>};
				},
				std::index_sequence_for<Types...>{}
			);
		}

		template <std::size_t Index, class Value, class Visit>
		constexpr static auto accept_value(const accept& self, Value value, Visit visit) -> accepted_type {
			const auto& acceptor = std::get<Index>(self.second);
			return acceptor(dictionary_tag{}, std::forward<Value>(value), visit);
		}

		std::tuple<accept_next<Meta, Types>...> second;
		accept_property_value<Meta, descriptor_type::discriminant, std::string, visit_subject_t<Meta>> accept_discriminant;
};

} // namespace js
