export module auto_js:union_.types;
import util;

namespace js {

// Discriminated union support in `std::variant` types. Alternatives which share a discriminant
// property name collect under the same discriminator.
template <auto Discriminant>
struct discriminated_union {};

export template <auto Discriminant, auto Value>
struct discriminated_alternative {
		constexpr discriminated_alternative(auto /*discriminant*/, auto /*value*/) {}
		constexpr static auto discriminator = type<discriminated_union<Discriminant>>;
		constexpr static auto discriminant_value = Value;
		constexpr static auto is_variant_discriminator = true;
};

template <class Discriminant, class Value>
discriminated_alternative(Discriminant, Value) -> discriminated_alternative<Discriminant{}, Value{}>;

} // namespace js
