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

		constexpr auto operator()(this auto& self, auto_tag auto tag, const auto& visit, auto&& subject)
			-> std::invoke_result_t<accept_type&, covariant_tag<decltype(tag)>, decltype(visit), decltype(subject)> {
			return util::invoke_as<accept_type>(self, covariant_tag{tag}, visit, std::forward<decltype(subject)>(subject));
		}

		constexpr auto operator()(Type&& target) -> Result {
			return Result{std::move(target)};
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

} // namespace js
