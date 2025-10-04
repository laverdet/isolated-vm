module;
#include <format>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js:union_.accept;
import :transfer;
import :union_.types;
import :variant.types;
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
	private:
		using accepted_type = std::variant<Types...>;
		using descriptor_type = union_of<accepted_type>;

	public:
		explicit constexpr accept(auto* transfer) :
				second{util::elide{[ & ] constexpr { return accept_next<Meta, Types>{transfer}; }}...},
				accept_discriminant{transfer} {}

		constexpr auto operator()(dictionary_tag /*tag*/, const auto& visit, auto&& dictionary) -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(dictionary), decltype(visit)>();
			auto discriminant_value = accept_discriminant(dictionary_tag{}, visit, dictionary);
			auto accept_alternative = alternatives.find(util::djb2_hash(discriminant_value));
			if (accept_alternative == nullptr) {
				throw std::logic_error{std::format("Unknown discriminant: {}", discriminant_value)};
			}
			return accept_alternative->second(*this, visit, std::forward<decltype(dictionary)>(dictionary));
		}

	private:
		template <class Value, class Visit>
		consteval static auto make_discriminant_map() {
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) consteval {
					return util::sealed_map{
						std::in_place,
						invoke(std::integral_constant<size_t, Index>{})...
					};
				},
				[]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) consteval {
					const auto& alternative = std::get<Index>(descriptor_type::alternatives);
					return std::pair{util::djb2_hash(alternative.discriminant), &accept_value<Index, Visit, Value>};
				},
				std::index_sequence_for<Types...>{}
			);
		}

		template <std::size_t Index, class Visit, class Value>
		// clang bug?
		/*constexpr*/ static auto accept_value(accept& self, Visit visit, Value value) -> accepted_type {
			auto& acceptor = std::get<Index>(self.second);
			return acceptor(dictionary_tag{}, visit, std::forward<Value>(value));
		}

		std::tuple<accept_next<Meta, Types>...> second;
		accept_property_value<Meta, descriptor_type::discriminant, std::string, typename Meta::visit_property_subject_type> accept_discriminant;
};

} // namespace js
