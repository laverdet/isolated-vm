module;
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
export module auto_js:variant.accept;
import :intrinsics.external;
import :transfer;
import :variant.types;
import util;

namespace js {

namespace {

// Returns a `util::type_pack` of `util::type_pack`'s: { primitives, objects, externals }
constexpr auto collect_alternatives_by_type = []<class Meta>(std::type_identity<Meta>, auto types) consteval {
	constexpr auto is_not_object_type = util::fn<[]<class Type>(std::type_identity<Type> /*type*/) -> bool {
		return !std::invocable<accept<Meta, Type>, object_tag, const std::monostate&, std::monostate>;
	}>;
	constexpr auto is_not_tagged_type = util::fn<util::overloaded{
		[]<class Type>(std::type_identity<Type> /*type*/) -> bool { return true; },
		[]<class Type>(std::type_identity<js::tagged_external<Type>> /*type*/) -> bool { return false; },
	}>;
	return util::pack_partition(types, is_not_object_type, is_not_tagged_type);
};

} // namespace

// Box variant alternative with `accept_value_from` interface
template <class Variant, class Type>
struct accept_with_variant {
		constexpr auto operator()(Type&& target) const -> Variant {
			return Variant{std::move(target)};
		}
};

// Distinguish primitives with `covariant_tag`
template <class Meta, class Variant, class Type>
struct accept_primitive_covariant
		: accept<Meta, Type>,
			accept_with_variant<Variant, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		using accept_with_variant<Variant, Type>::operator();
		constexpr auto operator()(this const auto& self, auto_tag auto tag, auto& visit, auto&& subject)
			-> std::invoke_result_t<const accept_type&, decltype(covariant_tag{tag}), decltype(visit), decltype(subject)> {
			return util::invoke_as<accept_type>(self, covariant_tag{tag}, visit, std::forward<decltype(subject)>(subject));
		}
};

// We assume that object types can negotiate amongst themselves by tag overload precedence
template <class Meta, class Variant, class Type>
struct accept_object_covariant
		: accept<Meta, Type>,
			accept_with_variant<Variant, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		using accept_type::operator();
		using accept_with_variant<Variant, Type>::operator();
};

// Collect `accept_object_covariant` instantiations
template <class Meta, class Variant, class... Types>
struct accept_object_covariants : accept_object_covariant<Meta, Variant, Types>... {
		constexpr explicit accept_object_covariants(auto* transfer) :
				accept_object_covariant<Meta, Variant, Types>{transfer}... {}

		using accept_object_covariant<Meta, Variant, Types>::operator()...;
		// Ensure that this class has an `operator()` for the `using <...>::operator()` declarations
		auto operator()() const -> void
			requires false;
};

// Perform runtime checking of external types
// TODO: Merge this, somehow, with discriminated unions
template <class Meta, class Variant, class... Types>
struct accept_external_covariants;

template <class Meta, class Variant, class... Types>
struct accept_external_covariants<Meta, Variant, js::tagged_external<Types>...> {
		constexpr auto operator()(object_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const -> std::optional<Variant> {
			using result_type = std::optional<Variant>;
			auto try_accept = util::overloaded{
				[ & ]() -> result_type { return std::nullopt; },
				[ & ](this const auto& try_accept, auto type, auto... types) -> result_type {
					auto* external = subject.try_cast(type);
					if (external == nullptr) {
						return try_accept(types...);
					} else {
						return Variant{js::tagged_external{*external}};
					}
				}
			};
			const auto [... types ] = util::type_pack{type<Types>...};
			return try_accept(types...);
		}
};

// Negotiate covariance of object types and host objects
template <class Meta, class Variant, class Objects, class Externals>
struct accept_object_and_host_covariants;

// Special case for when there are no host object types. No extra runtime checking is needed.
template <class Meta, class Variant, class... Objects>
struct accept_object_and_host_covariants<Meta, Variant, util::type_pack<Objects...>, util::type_pack<>>
		: accept_object_covariants<Meta, Variant, Objects...> {
		using accept_object_covariants<Meta, Variant, Objects...>::accept_object_covariants;
};

// Perform runtime type checking for external types
template <class Meta, class Variant, class... Objects, class... Externals>
struct accept_object_and_host_covariants<Meta, Variant, util::type_pack<Objects...>, util::type_pack<Externals...>>
		: accept_object_covariants<Meta, Variant, Objects...>,
			accept_external_covariants<Meta, Variant, Externals...> {
		using accept_external_type = accept_external_covariants<Meta, Variant, Externals...>;
		using accept_object_type = accept_object_covariants<Meta, Variant, Objects...>;
		using accept_object_type::accept_object_type;

		constexpr auto operator()(this const auto& self, auto_tag<object_tag> auto tag, auto& visit, auto&& subject) -> Variant {
			auto maybe_result = util::invoke_as<accept_external_type>(self, tag, visit, std::forward<decltype(subject)>(subject));
			if (maybe_result) {
				return *std::move(maybe_result);
			} else {
				if constexpr (std::invocable<const accept_object_type&, decltype(tag), decltype(visit), decltype(subject)>) {
					return Variant{util::invoke_as<accept_object_type>(self, tag, visit, std::forward<decltype(subject)>(subject))};
				} else {
					throw js::type_error{u"Invalid object type"};
				}
			}
		}
};

// Delegate to `accept_primitive_covariant` and `accept_object_and_host_covariants`
template <class Meta, class Variant, class Types>
struct accept_covariants;

template <class Meta, class Variant, class... Types>
using accept_covariants_from_t = accept_covariants<Meta, Variant, type_t<collect_alternatives_by_type(type<Meta>, util::type_pack<Types...>{})>>;

template <class Meta, class Variant, class... Primitives, class Objects, class Externals>
struct accept_covariants<Meta, Variant, util::type_pack<util::type_pack<Primitives...>, Objects, Externals>>
		: accept_primitive_covariant<Meta, Variant, Primitives>...,
			accept_object_and_host_covariants<Meta, Variant, Objects, Externals> {
		constexpr explicit accept_covariants(auto* transfer) :
				accept_primitive_covariant<Meta, Variant, Primitives>{transfer}...,
				accept_object_and_host_covariants<Meta, Variant, Objects, Externals>{transfer} {}
		using accept_primitive_covariant<Meta, Variant, Primitives>::operator()...;
		using accept_object_and_host_covariants<Meta, Variant, Objects, Externals>::operator();
};

// Unpack `std::variant` alternative types and pass forward to `accept_covariants`
template <class Meta, class... Types>
	requires is_variant_v<Types...>
struct accept<Meta, std::variant<Types...>> : accept_covariants_from_t<Meta, std::variant<Types...>, Types...> {
		using accept_covariants_from_t<Meta, std::variant<Types...>, Types...>::accept_covariants_from_t;
};

} // namespace js
