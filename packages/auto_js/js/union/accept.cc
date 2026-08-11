export module auto_js:union_.accept;
export import :union_.types;
import :variant.accept;
import std;
import util;

namespace js {

// Discriminates union alternatives which share a discriminant property
template <class Meta, class Variant, auto Discriminant, class... Types>
struct accept_discriminator<Meta, Variant, discriminated_union<Discriminant>, Types...> {
	public:
		explicit constexpr accept_discriminator(auto* transfer) :
				alternatives_{util::elide{util::constructor<accept_value<Meta, Types>>, transfer}...},
				accept_discriminant_{transfer} {}

		template <class Subject, class Visit>
		constexpr auto operator()(std::type_identity<Subject> /*type*/, dictionary_tag tag, Visit& visit, const std::remove_cvref_t<Subject>& subject) const {
			auto discriminant = accept_discriminant_(tag, visit, subject);
			return discriminant
				? std::optional{[ this, discriminant = std::move(discriminant), &visit ](auto&& subject) -> auto {
						constexpr auto alternatives = make_discriminant_map<Visit, Subject>();
						const auto* alternative = alternatives.find(util::fnv1a_hash(std::string_view{*discriminant}));
						if (alternative == nullptr) {
							auto discriminant_u16 = transfer_strict<std::u16string>(*discriminant);
							throw js::type_error{u"Unknown discriminant: '" + discriminant_u16 + u"'"};
						}
						return alternative->second(*this, visit, std::forward<decltype(subject)>(subject));
					}}
				: std::nullopt;
		}

		consteval static auto types(auto recursive) -> auto {
			return (util::type_pack{} + ... + accept_value<Meta, Types>::types(recursive));
		}

	private:
		template <std::size_t Index, class Visit, class Subject>
		constexpr static auto accept_alternative(const accept_discriminator& self, Visit& visit, Subject subject) -> Variant {
			return std::get<Index>(self.alternatives_)(dictionary_tag{}, visit, std::forward<Subject>(subject));
		}

		template <class Visit, class Subject>
		consteval static auto make_discriminant_map() {
			constexpr auto [... indices ] = util::sequence<sizeof...(Types)>;
			return util::sealed_map{
				std::in_place,
				std::pair{
					util::fnv1a_hash(util::make_consteval_string_view(variant_discriminator<Types>::value.discriminant_value)),
					&accept_alternative<indices, Visit, Subject>,
				}...,
			};
		}

		std::tuple<accept_value<Meta, Types>...> alternatives_;
		accept_property_value<Meta, Discriminant, std::optional<std::string>, typename Meta::visit_property_subject_type> accept_discriminant_;
};

} // namespace js
