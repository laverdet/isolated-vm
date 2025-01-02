module;
#include <format>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.js:union_.accept;
import :tag;
import :transfer;
import :union_.types;
import :variant.types;
import ivm.utility;

namespace ivm::js {

// Suppress `std::variant` visitor
template <class... Types>
	requires std::destructible<union_of<std::variant<Types...>>>
struct is_variant<Types...> : std::bool_constant<false> {};

// Acceptor for discriminated union/variant types
template <class Meta, class... Types>
	requires std::negation_v<is_variant<Types...>>
struct accept<Meta, std::variant<Types...>> {
	public:
		using accepted_type = std::variant<Types...>;
		using descriptor_type = union_of<accepted_type>;

		explicit constexpr accept(const visit_root<Meta>& visit) :
				second{accept_next<Meta, Types>{visit}...},
				accept_discriminant{visit} {}
		constexpr accept(int /*dummy*/, const visit_root<Meta>& visit, const auto_accept auto& /*accept*/) :
				second{accept_next<Meta, Types>{visit}...},
				accept_discriminant{visit} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(dictionary), decltype(visit)>();
			auto discriminant_value = accept_discriminant(dictionary_tag{}, dictionary, visit);
			auto accept_alternative = alternatives.get(discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error(std::format("Unknown discriminant: {}", discriminant_value));
			}
			return (*accept_alternative)(*this, std::forward<decltype(dictionary)>(dictionary), visit);
		}

		template <class Value, class Visit>
		consteval static auto make_discriminant_map() {
			using acceptor_type = accepted_type (*)(const accept&, Value, Visit);
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) consteval {
					return util::prehashed_string_map{invoke(std::integral_constant<size_t, Index>{})...};
				},
				[]<size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& alternative = std::get<Index>(descriptor_type::alternatives);
					acceptor_type acceptor = [](const accept& self, Value value, Visit visit) -> accepted_type {
						const auto& accept = std::get<Index>(self.second);
						return accept(dictionary_tag{}, std::forward<Value>(value), visit);
					};
					return std::pair{alternative.discriminant, acceptor};
				},
				std::index_sequence_for<Types...>{}
			);
		}

	private:
		std::tuple<accept_next<Meta, Types>...> second;
		accept<Meta, value_by_key<descriptor_type::discriminant, std::string, visit_subject_t<Meta>>> accept_discriminant;
};

} // namespace ivm::js
