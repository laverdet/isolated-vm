module;
#include <boost/variant.hpp>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
export module ivm.value:accept;
import ivm.utility;
import :tag;
import :visit;

namespace ivm::value {

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// Convert an existing acceptor into a new one which accepts a different type
export template <class Next, class Meta, class Type>
auto make_accept(const accept<Meta, Type>& accept_from) {
	using accept_type = accept<Meta, typename Meta::template wrap<Next>>;
	if constexpr (std::is_default_constructible_v<accept_type>) {
		return accept_type{};
	} else {
		return accept_type{accept_from};
	}
}

// Default acceptor just forwards the given value directly to the underlying type's constructor
template <class Meta, class Type>
struct accept {
		constexpr auto operator()(tag_for_t<Type> /*tag*/, convertible_without_narrowing<Type> auto&& value) {
			return Type{std::forward<decltype(value)>(value)};
		}
};

// `undefined` -> `std::monostate`
template <class Meta>
struct accept<Meta, std::monostate> {
		constexpr auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) {
			return std::monostate{};
		}
};

// `null` -> `std::nullptr_t`
template <class Meta>
struct accept<Meta, std::nullptr_t> {
		constexpr auto operator()(null_tag /*tag*/, auto&& /*null*/) {
			return std::nullptr_t{};
		}
};

// Accepting a `std::tuple` unfolds from an accepted vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
		constexpr auto operator()(vector_tag /*tag*/, auto value) -> std::tuple<Types...> {
			if (std::size(value) < sizeof...(Types)) {
				throw std::logic_error("Too small");
			}
			auto it = std::begin(value);
			return {invoke_visit(*it++, make_accept<Types>(*this))...};
		}
};

// Helper for `accept<std::variant<Types...>>`
template <class Meta, class Type, class Result>
struct accept_as : accept<Meta, Type> {
		constexpr auto operator()(auto_tag auto tag, auto&& value) -> Result
			// nb: This doesn't use `make_accept` since the outer type is still wrapped
			requires std::is_invocable_v<accept<Meta, Type>, decltype(tag), decltype(value)> {
			return accept<Meta, Type>::operator()(tag, std::forward<decltype(value)>(value));
		}
};

// Accepting a `std::variant` will delegate to each underlying type's acceptor and box the result
// into the variant.
template <class Meta, class... Types>
struct accept<Meta, std::variant<Types...>> : accept_as<Meta, Types, std::variant<Types...>>... {
		using accept_as<Meta, Types, std::variant<Types...>>::operator()...;
};

// Non-recursive `boost::variant` has the same behavior as `std::variant`
template <class Meta, class... Types>
struct accept<Meta, boost::variant<Types...>> : accept_as<Meta, Types, boost::variant<Types...>>... {
		using accept_as<Meta, Types, boost::variant<Types...>>::operator()...;
};

// Helper which holds a recursive variant followed by its alternatives
template <class Variant, class... Types>
struct variant_helper {};

// Helper which extracts recursive variant alternative types
template <class Type, class... Types>
using recursive_variant = boost::variant<boost::detail::variant::recursive_flag<Type>, Types...>;

// Helper which substitutes recursive variant alternative types
template <class Variant, class Type>
using substitute_recursive = typename boost::detail::variant::substitute<Type, Variant, boost::recursive_variant_>::type;

// Recursive variant acceptor
template <class Meta, class Variant, class... Types>
struct accept<Meta, variant_helper<Variant, Types...>>
		: accept_as<Meta, substitute_recursive<Variant, Types>, Variant>... {
		using accept_as<Meta, substitute_recursive<Variant, Types>, Variant>::operator()...;
};

// Entry for `boost::make_recursive_variant`
template <class Meta, class First, class... Types>
struct accept<Meta, recursive_variant<First, Types...>>
		: accept<Meta, variant_helper<recursive_variant<First, Types...>, First, Types...>> {
		using accept<Meta, variant_helper<recursive_variant<First, Types...>, First, Types...>>::operator();
};

} // namespace ivm::value
