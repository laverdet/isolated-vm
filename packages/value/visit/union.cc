module;
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.value:union_of;
import :tag;
import :variant;
import :visit;
import ivm.utility;

namespace ivm::value {

// Specialize to turn `std::variant` into a discriminated union
export template <class Type>
struct union_of;

// Holds typed union alternative w/ discriminant
export template <class Type>
struct alternative {
		constexpr alternative(const std::string& discriminant) :
				discriminant{discriminant} {}

		std::string discriminant;
};

// Suppress `std::variant` visitor
template <class... Types>
	requires std::destructible<union_of<std::variant<Types...>>>
struct is_variant<Types...> : std::bool_constant<false> {};

// Acceptor for discriminated union/variant types
template <class Meta, class... Types>
	requires std::negation_v<is_variant<Types...>>
struct accept<Meta, std::variant<Types...>> : accept<Meta, void> {
	public:
		using accepted_type = std::variant<Types...>;
		using descriptor_type = union_of<accepted_type>;

		constexpr accept(const auto_visit auto& visit) :
				accept_string{visit},
				second{accept_next<Meta, Types>{visit}...},
				visit_discriminant{visit} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept*/) :
				accept_string{visit},
				second{accept_next<Meta, Types>{visit}...},
				visit_discriminant{visit} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(dictionary), decltype(visit)>();
			auto discriminant_value = visit_discriminant(dictionary, visit, accept_string);
			if (!discriminant_value) {
				throw std::logic_error("Missing discriminant");
			}
			auto accept_alternative = alternatives.get(*discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error(std::format("Unknown discriminant: {}", *discriminant_value));
			}
			return (*accept_alternative)(*this, std::forward<decltype(dictionary)>(dictionary), visit);
		}

		template <class Value, class Visit>
		consteval static auto make_discriminant_map() {
			using acceptor_type = accepted_type (*)(const accept&, Value, Visit);
			return std::invoke(
				[]<size_t... Index>(std::index_sequence<Index...> /*indices*/) consteval {
					return util::prehashed_string_map{std::invoke(
						[](const auto& alternative) constexpr {
							acceptor_type acceptor = [](const accept& self, Value value, Visit visit) -> accepted_type {
								const auto& accept = std::get<Index>(self.second);
								return accept(dictionary_tag{}, std::forward<Value>(value), visit);
							};
							return std::pair{alternative.discriminant, acceptor};
						},
						std::get<Index>(descriptor_type::alternatives)
					)...};
				},
				std::index_sequence_for<Types...>{}
			);
		}

	private:
		accept_next<Meta, std::string> accept_string;
		std::tuple<accept_next<Meta, Types>...> second;
		visit_key<Meta, descriptor_type::discriminant> visit_discriminant;
};

} // namespace ivm::value
