export module auto_js:union_.accept;
export import :union_.types;
import :variant.accept;
import std;
import util;

namespace js {

// Runtime representation & lookup keys for each supported discriminant value type
template <class Type>
struct accept_discriminant_value;

template <class Type>
using accept_discriminant_value_t = accept_discriminant_value<
	typename std::remove_cvref_t<decltype(variant_discriminator<Type>::value.discriminant_value)>::value_type>;

// String discriminants
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
template <class Char, std::size_t Extent>
struct accept_discriminant_value<Char[ Extent ]> {
		using type = std::basic_string<Char>;

		constexpr static auto key_of(const type& value) -> std::uint32_t {
			return util::fnv1a_hash32(std::basic_string_view<Char>{value});
		}

		consteval static auto key_of(auto discriminant_value) -> std::uint32_t {
			return util::fnv1a_hash32(util::make_consteval_string_view(discriminant_value));
		}

		static auto describe(const type& value) -> std::u16string {
			return transfer_strict<std::u16string>(value);
		}
};

// Boolean discriminants
template <>
struct accept_discriminant_value<bool> {
		using type = bool;

		constexpr static auto key_of(bool value) -> std::uint32_t {
			return std::uint32_t{value};
		}

		static auto describe(bool value) -> std::u16string {
			return value ? std::u16string{u"true"} : std::u16string{u"false"};
		}
};

// Discriminates union alternatives which share a discriminant property
template <class Meta, class Variant, auto Discriminant, class... Types>
struct accept_discriminator<Meta, Variant, discriminated_union<Discriminant>, Types...> {
	private:
		using discriminant_value_type = accept_discriminant_value_t<Types...[ 0 ]>;
		using discriminant_type = discriminant_value_type::type;
		static_assert(
			(... && std::same_as<discriminant_type, typename accept_discriminant_value_t<Types>::type>),
			"Union alternatives must share a discriminant value type"
		);

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
						const auto* alternative = alternatives.find(discriminant_value_type::key_of(*discriminant));
						if (alternative == nullptr) {
							throw js::type_error{u"Unknown discriminant: '" + discriminant_value_type::describe(*discriminant) + u"'"};
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
					accept_discriminant_value_t<Types>::key_of(variant_discriminator<Types>::value.discriminant_value),
					&accept_alternative<indices, Visit, Subject>,
				}...,
			};
		}

		std::tuple<accept_value<Meta, Types>...> alternatives_;
		accept_property_value<Meta, Discriminant, std::optional<discriminant_type>, typename Meta::visit_property_subject_type> accept_discriminant_;
};

} // namespace js
