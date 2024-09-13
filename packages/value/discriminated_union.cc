module;
#include <format>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>
export module ivm.value:discriminated_union;
import :accept;
import :tag;
import ivm.utility;

namespace ivm::value {

// Each discriminated variant descriptor should inherit from this
export template <class Meta, class Value, class Type>
struct discriminated_alternatives {
		using variant_accept = accept<Meta, Type>;
		using acceptor_type = Type (*)(const variant_accept&, const Value&);
		constexpr static bool is_discriminated_union = true;

		template <class Alternative>
		constexpr static auto accept(const variant_accept& accept, const Value& value) -> Type {
			// Skip accept for `std::nullopt_t`, which is instantiated in the `discriminated_union`
			// acceptor requirement
			if constexpr (std::is_same_v<Value, std::nullopt_t>) {
				// `Type` may not be default constructible
				throw std::domain_error("Nothing");
			} else {
				return make_accept<Alternative>(accept)(dictionary_tag{}, value);
			}
		}
};

// Acceptor for discriminated variant types
template <class Meta, class... Types>
	requires discriminated_union<Meta, std::nullopt_t, std::variant<Types...>>::is_discriminated_union
struct accept<Meta, std::variant<Types...>> {
		using accept_type = std::variant<Types...>;

		constexpr auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> accept_type {
			using value_type = std::decay_t<decltype(dictionary)>;
			using descriptor_type = discriminated_union<Meta, value_type, accept_type>;
			auto alternatives = make_discriminant_map<value_type>();
			auto discriminant_value = invoke_visit(
				dictionary.get(descriptor_type::discriminant),
				make_accept<std::string>(*this)
			);
			auto accept_alternative = alternatives.get(discriminant_value);
			if (accept_alternative == nullptr) {
				throw std::logic_error(std::format("Unknown discriminant: {}", discriminant_value));
			}
			return (*accept_alternative)(*this, std::forward<decltype(dictionary)>(dictionary));
		}

		template <class Value>
		consteval static auto make_discriminant_map() {
			using descriptor_type = discriminated_union<Meta, Value, accept_type>;
			using acceptor_type = descriptor_type::acceptor_type;
			auto alternatives = descriptor_type::alternatives;
			return prehashed_string_map<acceptor_type, alternatives.size()>{alternatives};
		}
};

} // namespace ivm::value
