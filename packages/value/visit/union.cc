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
struct accept<Meta, std::variant<Types...>> {
		using accepted_type = std::variant<Types...>;
		using descriptor_type = union_of<accepted_type>;

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(dictionary)>();
			auto discriminant_value = invoke_visit(dictionary.get(descriptor_type::discriminant), accept_key_);
			auto accept_alternative = alternatives.get(discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error(std::format("Unknown discriminant: {}", discriminant_value));
			}
			return (*accept_alternative)(std::forward<decltype(dictionary)>(dictionary), *this);
		}

		template <class Value>
		consteval static auto make_discriminant_map() {
			using acceptor_type = accepted_type (*)(Value, const accept&);
			return util::prehashed_string_map{std::invoke(
				[]<size_t... Index>(std::index_sequence<Index...> /*indices*/) consteval {
					return std::array{std::invoke(
						[](const auto& alternative) {
							acceptor_type acceptor = [](Value value, const accept& self) -> accepted_type {
								const auto& accept = std::get<Index>(self.acceptors_);
								return accept(dictionary_tag{}, std::forward<Value>(value));
							};
							return std::pair{alternative.discriminant, acceptor};
						},
						std::get<Index>(descriptor_type::alternatives)
					)...};
				},
				std::index_sequence_for<Types...>{}
			)};
		}

	private:
		accept_next<Meta, std::string> accept_key_;
		std::tuple<accept_next<Meta, Types>...> acceptors_;
};

} // namespace ivm::value
