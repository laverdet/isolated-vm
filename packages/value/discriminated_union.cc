module;
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.value:discriminated_union;
import :tag;
import :variant;
import :visit;
import ivm.utility;

namespace ivm::value {

// Specialize to override the acceptor for `std::variant`
export template <class Meta, class Type>
struct discriminated_union;

// Each discriminated variant descriptor should inherit from this
export template <class Meta, class Type>
struct discriminated_alternatives;

// Specialization to extract tuple types
export template <class Meta, class Value, class Type>
struct discriminated_alternatives<std::tuple<Meta, Value>, Type> {
		using variant_accept_type = accept<Meta, Type>;
		using alternative_type = Type (*)(const Value&, const variant_accept_type&);

		template <class Alternative>
		constexpr static auto alternative(const Value& value, const variant_accept_type& accept) -> Type {
			// Skip accept for `std::nullopt_t`, which is instantiated in the `discriminated_union`
			// acceptor requirement
			if constexpr (std::is_same_v<Value, std::nullopt_t>) {
				std::unreachable();
			} else {
				return make_accept<Alternative>(accept)(dictionary_tag{}, value);
			}
		}
};

// Suppress `std::variant` visitor
template <class... Types>
	requires std::destructible<discriminated_union<std::tuple<void, std::nullopt_t>, std::variant<Types...>>>
struct is_variant<Types...> {
		constexpr static bool value = false;
};

// Acceptor for discriminated variant types
template <class Meta, class... Types>
	requires std::negation_v<is_variant<Types...>>
struct accept<Meta, std::variant<Types...>> {
		using accept_type = std::variant<Types...>;

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> accept_type {
			using value_type = std::decay_t<decltype(dictionary)>;
			using descriptor_type = discriminated_union<std::tuple<Meta, value_type>, accept_type>;
			auto alternatives = make_discriminant_map<value_type>();
			auto discriminant_value = invoke_visit(
				dictionary.get(descriptor_type::discriminant),
				make_accept<std::string>(*this)
			);
			auto accept_alternative = alternatives.get(discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error(std::format("Unknown discriminant: {}", discriminant_value));
			}
			return (*accept_alternative)(std::forward<decltype(dictionary)>(dictionary), *this);
		}

		template <class Value>
		consteval static auto make_discriminant_map() {
			using descriptor_type = discriminated_union<std::tuple<Meta, Value>, accept_type>;
			using alternative_type = descriptor_type::alternative_type;
			auto alternatives = descriptor_type::alternatives;
			return util::prehashed_string_map<alternative_type, alternatives.size()>{alternatives};
		}
};

} // namespace ivm::value
