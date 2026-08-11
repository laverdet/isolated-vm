export module auto_js:external.accept;
import :intrinsics.external;
import :variant.accept;
import std;
import util;

namespace js {

// Discriminates a `tagged_external` alternative with a runtime type check
template <class Meta, class Variant, class Type>
struct accept_discriminator<Meta, Variant, external_alternative<Type>, tagged_external<Type>> {
		explicit constexpr accept_discriminator(auto* /*transfer*/) {}

		template <class Subject>
		constexpr auto operator()(std::type_identity<Subject> /*type*/, object_tag /*tag*/, visit_holder /*visit*/, const std::remove_cvref_t<Subject>& subject) const
			requires requires { subject.try_cast(type<Type>); } {
			auto* external = subject.try_cast(type<Type>);
			return external
				? std::optional{[ external ](auto&& /*subject*/) -> Variant {
						return Variant{tagged_external{*external}};
					}}
				: std::nullopt;
		}

		consteval static auto types(auto /*recursive*/) -> auto {
			return util::type_pack{};
		}
};

} // namespace js
