export module auto_js:union_.visit;
export import :union_.types;
import :struct_.visit;
import :variant.types;
import std;
import util;

namespace js {

// Discriminated union alternatives which also define a struct template are visitable.
template <class Type>
concept discriminated_struct = transferable_struct<Type> && requires { variant_discriminator<Type>::value; };

// Prepends the union discriminant as a constant property of the alternative's own struct template.
template <class Type>
struct union_struct_properties {
	private:
		constexpr static auto size = std::tuple_size_v<std::remove_cvref_t<decltype(struct_properties<Type>::properties.as_tuple())>>;

	public:
		constexpr static auto properties = [] consteval {
			constexpr auto alternative = variant_discriminator<Type>::value;
			constexpr auto [... indices ] = util::sequence<size>;
			return js::struct_template{
				js::struct_constant{alternative.discriminant_name, alternative.discriminant_value},
				std::get<indices>(struct_properties<Type>::properties.as_tuple())...
			};
		}();
};

// Visitor for discriminated union alternatives, which includes the discriminant property
template <class Meta, discriminated_struct Type>
struct visit<Meta, Type> : visit_struct_properties_t<Meta, union_struct_properties<Type>> {
		using visit_struct_properties_t<Meta, union_struct_properties<Type>>::visit_struct_properties_t;
};

} // namespace js
