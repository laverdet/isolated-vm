module;
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
export module auto_js:union_.accept;
import :intrinsics.error;
import :transfer;
import :union_.types;
import :variant.types;
import util;

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
				second{util::elide{util::constructor<accept_value<Meta, Types>>, transfer}...},
				accept_discriminant{transfer} {}

		constexpr auto operator()(dictionary_tag /*tag*/, auto& visit, auto&& subject) const -> accepted_type {
			auto alternatives = make_discriminant_map<decltype(subject), decltype(visit)>();
			auto discriminant_value = accept_discriminant(dictionary_tag{}, visit, subject);
			auto accept_alternative = alternatives.find(util::fnv1a_hash(std::basic_string_view{discriminant_value}));
			if (accept_alternative == nullptr) {
				auto discriminant_u16 = transfer_strict<std::u16string>(discriminant_value);
				throw js::type_error{u"Unknown discriminant: '" + discriminant_u16 + u"'"};
			}
			return accept_alternative->second(*this, visit, std::forward<decltype(subject)>(subject));
		}

	private:
		template <class Value, class Visit>
		consteval static auto make_discriminant_map() {
			const auto [... indices ] = util::sequence<sizeof...(Types)>;
			return util::sealed_map{
				std::in_place,
				[ & ]() constexpr -> auto {
					const auto& alternative = std::get<indices>(descriptor_type::alternatives);
					return std::pair{util::fnv1a_hash(std::basic_string_view{alternative.discriminant}), &accept_alternative<indices, Visit, Value>};
				}()...,
			};
		}

		template <std::size_t Index, class Visit, class Value>
		// clang bug?
		// maybe: https://github.com/llvm/llvm-project/issues/73232
		/*constexpr*/ static auto accept_alternative(const accept& self, Visit visit, Value value) -> accepted_type {
			return std::get<Index>(self.second)(dictionary_tag{}, visit, std::forward<Value>(value));
		}

		std::tuple<accept_value<Meta, Types>...> second;
		accept_property_value<Meta, descriptor_type::discriminant, std::string, typename Meta::visit_property_subject_type> accept_discriminant;
};

} // namespace js
