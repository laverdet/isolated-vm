module;
#include <boost/variant.hpp>
#include <concepts>
#include <optional>
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
constexpr auto make_accept(const accept<Meta, Type>& accept_from) {
	using accept_type = accept<Meta, typename Meta::template wrap<Next>>;
	if constexpr (std::constructible_from<accept_type, const accept<Meta, Type>&>) {
		return accept_type{accept_from};
	} else {
		static_assert(std::is_default_constructible_v<accept_type>);
		return accept_type{};
	}
}

// Delegate to another acceptor by a different type. Does not wrap in the same way `make_accept`
// does.
export template <class Next, class Meta, class Type>
constexpr auto delegate_accept(
	const accept<Meta, Type>& /*accept_from*/,
	auto tag,
	auto&& value,
	auto&&... accept_args
) -> decltype(auto) {
	using accept_type = accept<Meta, Next>;
	return accept_type{
		std::forward<decltype(accept_args)>(accept_args)...
	}(
		tag,
		std::forward<decltype(value)>(value)
	);
}

// Default acceptor just forwards the given value directly to the underlying type's constructor
template <class Meta, class Type>
struct accept {
		constexpr auto operator()(tag_con_for_t<Type> /*tag*/, auto&& value) const {
			if constexpr (util::is_convertible_without_narrowing_v<decltype(value), Type>) {
				return Type{std::forward<decltype(value)>(value)};
			} else {
				return static_cast<Type>(std::forward<decltype(value)>(value));
			}
		}
};

// `undefined` -> `std::monostate`
template <class Meta>
struct accept<Meta, std::monostate> {
		constexpr auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const {
			return std::monostate{};
		}
};

// `null` -> `std::nullptr_t`
template <class Meta>
struct accept<Meta, std::nullptr_t> {
		constexpr auto operator()(null_tag /*tag*/, auto&& /*null*/) const {
			return std::nullptr_t{};
		}
};

// Accepting a `std::tuple` unfolds from an accepted vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
		constexpr auto operator()(vector_tag /*tag*/, auto&& value) const -> std::tuple<Types...> {
			if (std::size(value) < sizeof...(Types)) {
				throw std::logic_error("Too small");
			}
			auto it = std::begin(value);
			return {invoke_visit(*it++, make_accept<Types>(*this))...};
		}
};

// `std::optional` allows `undefined` in addition to the next acceptor
template <class Meta, class Type>
struct accept<Meta, std::optional<Type>> : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		constexpr auto operator()(auto_tag auto tag, auto&& value) const -> std::optional<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			return {accept_type::operator()(tag, std::forward<decltype(value)>(value))};
		}
		constexpr auto operator()(undefined_tag /*tag*/, auto&& /*value*/) const -> std::optional<Type> {
			return std::nullopt;
		}
};

// Specialize to override the acceptor for `std::variant`
export template <class Meta, class Value, class Type>
struct discriminated_union {
		constexpr static bool is_discriminated_union = false;
};

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
	requires(!discriminated_union<Meta, std::nullopt_t, std::variant<Types...>>::is_discriminated_union)
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
struct accept<Meta, recursive_variant<First, Rest...>>
		: accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>> {
		using accept<Meta, variant_helper<recursive_variant<First, Rest...>, First, Rest...>>::operator();
};

} // namespace ivm::value
