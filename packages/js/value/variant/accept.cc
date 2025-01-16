module;
#include <type_traits>
#include <utility>
#include <variant>
export module isolated_js.variant.accept;
import isolated_js.tag;
import isolated_js.transfer;
import isolated_js.variant.types;

namespace js {

// Covariant `accept` helper for variant-like types
template <class Type, class Result>
struct covariant_subject;

// Boxes the result of the underlying acceptor into the variant
template <class Meta, class Type, class Result>
struct accept<Meta, covariant_subject<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> Result
			requires std::is_invocable_v<accept<Meta, Type>, decltype(tag), decltype(value), decltype(rest)...> {
			return accept<Meta, Type>::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}
};

// Tagged primitives only accept their covariant type
template <class Meta, class Type, class Result>
	requires std::negation_v<std::is_same<tag_for_t<Type>, void>>
struct accept<Meta, covariant_subject<Type, Result>> : accept<Meta, Type> {
		using accept<Meta, Type>::accept;

		constexpr auto operator()(tag_for_t<Type> tag, auto&& value) const -> Result {
			return accept<Meta, Type>::operator()(tag, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor via the
// `covariant_subject` helper `accept` specialization.
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>>
		: accept<Meta, covariant_subject<Types, std::variant<Types...>>>... {
		explicit constexpr accept(const visit_root<Meta>& visit) :
				accept<Meta, covariant_subject<Types, std::variant<Types...>>>{visit}... {}
		constexpr accept(int dummy, const visit_root<Meta>& visit, const auto_accept auto& accept_) :
				accept<Meta, covariant_subject<Types, std::variant<Types...>>>{dummy, visit, accept_}... {}
		using accept<Meta, covariant_subject<Types, std::variant<Types...>>>::operator()...;
};

// Recursive `boost::variant` acceptor
template <class Meta, class Variant, class... Types>
struct accept<Meta, variant_of<Variant, Types...>>
		: accept<Meta, covariant_subject<substitute_recursive<Variant, Types>, Variant>>... {
		explicit constexpr accept(const visit_root<Meta>& visit) :
				accept<Meta, covariant_subject<substitute_recursive<Variant, Types>, Variant>>{visit}... {}
		constexpr accept(int dummy, const visit_root<Meta>& visit, const auto_accept auto& accept_) :
				accept<Meta, covariant_subject<substitute_recursive<Variant, Types>, Variant>>{dummy, visit, accept_}... {}
		using accept<Meta, covariant_subject<substitute_recursive<Variant, Types>, Variant>>::operator()...;
};

// `accept` entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Rest>
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept<Meta, variant_of<recursive_variant<First, Rest...>, First, Rest...>>::accept;
};

} // namespace js
