module;
#include <expected>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js:struct_.accept;
import :intrinsics.error;
import :property;
import :struct_.types;
import :transfer;
import ivm.utility;

namespace js {

// Property acceptor and setter delegate for one property entry
template <class Meta, class Property>
struct accept_object_property {
	public:
		constexpr accept_object_property(auto* transfer, Property property) :
				accept_{transfer},
				set_{property.set} {}

		constexpr auto operator()(auto& visit, auto&& subject, auto& target) const -> void {
			// nb: We `std::forward` the value to *each* setter. This allows the setters to pick an
			// lvalue object apart member by member if it wants.
			auto value = accept_(dictionary_tag{}, visit, std::forward<decltype(subject)>(subject));
			if (value) {
				set_(target, *std::move(value));
			} else {
				if constexpr (!std::is_nothrow_constructible_v<member_type, std::nullopt_t>) {
					// If the value type can be constructed from `std::nullopt` then a missing property is
					// allowed. In this case the setter is not invoked at all, which could in theory be used
					// to distinguish between `undefined` and missing properties.
					auto name_u16 = transfer_strict<std::u16string>(Property::name);
					throw js::type_error{u"Missing required property: '" + name_u16 + u"'"};
				}
			}
		}

	private:
		using setter_type = decltype(std::declval<Property>().set);
		using member_type = setter_type::value_type;
		using value_type = std::expected<member_type, undefined_in_tag>;
		using accept_type = accept_property_value<Meta, Property::name, value_type, typename Meta::visit_property_subject_type>;

		[[no_unique_address]] accept_type accept_;
		[[no_unique_address]] setter_type set_;
};

// Helper to unpack `properties` tuple, construct a new target struct, and invoke each instance of
// `accept_object_property`.
template <class Meta, class Type, class Properties>
struct accept_struct_properties;

template <class Meta, class Type>
using accept_struct_properties_t =
	accept_struct_properties<Meta, Type, std::remove_cvref_t<decltype(struct_properties<Type>::properties)>>;

template <class Meta, class Type, class... Property>
struct accept_struct_properties<Meta, Type, js::struct_template<Property...>> {
	private:
		using descriptor_type = struct_properties<Type>;
		using properties_type = std::tuple<accept_object_property<Meta, Property>...>;

	public:
		explicit constexpr accept_struct_properties(auto* transfer) :
				properties{[ & ]() -> properties_type {
					const auto [... indices ] = util::sequence<sizeof...(Property)>;
					return {util::elide(
						util::constructor<accept_object_property<Meta, Property...[ indices ]>>,
						transfer,
						std::get<indices>(descriptor_type::properties)
					)...};
				}()} {}

		constexpr auto operator()(undefined_tag /*tag*/, auto& /*visit*/, const auto& /*subject*/) const -> Type
			requires(std::is_default_constructible_v<Type> && descriptor_type::defaultable) {
			return Type{};
		}

		constexpr auto operator()(dictionary_tag /*tag*/, auto& visit, auto&& subject) const -> Type {
			Type target;
			const auto& [... properties_n ] = properties;
			// NOLINTNEXTLINE(bugprone-use-after-move)
			(..., properties_n(visit, std::forward<decltype(subject)>(subject), target));
			return target;
		}

	private:
		properties_type properties;
};

// Struct acceptor
template <class Meta, transferable_struct Type>
struct accept<Meta, Type> : accept_struct_properties_t<Meta, Type> {
		using accept_struct_properties_t<Meta, Type>::accept_struct_properties_t;
};

} // namespace js
