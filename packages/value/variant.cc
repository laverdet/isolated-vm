module;
#include <boost/variant.hpp>
#include <string>
#include <variant>
export module ivm.value:variant;
import :date;
import :dictionary;
import :primitive;
import :tag;
import :visit;

namespace ivm::value {

// Specialize in order to disable `std::variant` visitor
export template <class... Types>
struct is_variant {
		constexpr static bool value = true;
};

export template <class... Types>
constexpr bool is_variant_v = is_variant<Types...>::value;

// Covariant helper for variant-like types
template <class Type, class Result>
struct covariant {};

template <class Meta, class Type, class Result>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(auto_tag auto tag, auto&& value) const -> Result
			requires std::is_invocable_v<accept<Meta, Type>, decltype(tag), decltype(value)> {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value));
		}
};

template <class Meta, class Type, class Result>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<Meta, covariant<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(tag_for_t<Type> tag, auto&& value) const -> Result {
			const accept<Meta, Type>& parent = *this;
			return parent(tag, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor and box the result
// into the variant.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>> : accept<Meta, covariant<Types, std::variant<Types...>>>... {
		using accept<Meta, covariant<Types, std::variant<Types...>>>::operator()...;
};

// Non-recursive `boost::variant` has the same behavior as `std::variant`
template <class Meta, class... Types>
struct accept<Meta, boost::variant<Types...>> : accept<Meta, covariant<Types, boost::variant<Types...>>>... {
		using accept<Meta, covariant<Types, boost::variant<Types...>>>::operator()...;
};

// Helper which holds a recursive variant followed by its alternatives
template <class Variant, class... Types>
struct variant_helper {};

// Helper which extracts recursive variant alternative types
template <class First, class... Rest>
using recursive_variant = boost::variant<boost::detail::variant::recursive_flag<First>, Rest...>;

// Helper which substitutes recursive variant alternative types
template <class Variant, class Type>
using substitute_recursive = typename boost::detail::variant::substitute<Type, Variant, boost::recursive_variant_>::type;

// Recursive variant acceptor
template <class Meta, class Variant, class... Types>
struct accept<Meta, variant_helper<Variant, Types...>>
		: accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>... {
		using accept<Meta, covariant<substitute_recursive<Variant, Types>, Variant>>::operator()...;
};

// Entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
	requires is_variant_v<First, Rest...>
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>>::operator();
};

// Visiting a `boost::variant` visits the underlying member
template <class... Types>
	requires is_variant_v<Types...>
struct visit<boost::variant<Types...>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return boost::apply_visitor(
				[ & ](auto&& value) constexpr {
					return invoke_visit(*this, std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class... Types>
	requires is_variant_v<Types...>
struct visit<std::variant<Types...>> : visit<void> {
		using visit<void>::visit;
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return std::visit(
				[ & ](auto&& value) constexpr {
					return invoke_visit(*this, std::forward<decltype(value)>(value), accept);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

} // namespace ivm::value
