module;
#include <utility>
#include <variant>
export module isolated_js:variant.accept;
import :transfer;
import :variant.types;
import ivm.utility;

namespace js {

// Covariant `accept` helper for variant-like types
template <class Meta, class Type, class Result>
struct accept_covariant;

// Boxes the result of the underlying acceptor into the variant
template <class Meta, class Type, class Result>
struct accept_covariant : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		constexpr auto operator()(auto_tag auto tag, const auto& visit, auto&& value) -> Result
			requires std::is_invocable_v<accept_type&, decltype(covariant_tag{tag}), decltype(visit), decltype(value)> {
			return util::invoke_as<accept_type>(*this, covariant_tag{tag}, visit, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor via
// `accept_covariant`.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>> : accept_covariant<Meta, Types, std::variant<Types...>>... {
		explicit constexpr accept(auto* transfer) :
				accept_covariant<Meta, Types, std::variant<Types...>>{transfer}... {}
		using accept_covariant<Meta, Types, std::variant<Types...>>::operator()...;
};

// Recursive `boost::variant` acceptor
template <class Meta, class Variant>
struct accept_recursive_variant;

template <class Meta, class Variant, class... Types>
struct accept_recursive_variant<Meta, variant_of<Variant, Types...>>
		: accept_covariant<Meta, substitute_recursive<Variant, Types>, Variant>... {
		explicit constexpr accept_recursive_variant(auto* transfer) :
				accept_covariant<Meta, substitute_recursive<Variant, Types>, Variant>{transfer}... {}
		using accept_covariant<Meta, substitute_recursive<Variant, Types>, Variant>::operator()...;
};

// `accept` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept_recursive_variant<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept_recursive_variant<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>::accept_recursive_variant;
};

} // namespace js
