export module auto_js:variant.accept;
import :intrinsics.external;
import :transfer;
import :variant.types;
import std;
import util;

namespace js {

// Returns a `util::type_pack` of `util::type_pack`'s: { plain, discriminators }.
// 'discriminators' is pack of packs `{ discriminator, subjects... }`.
constexpr auto collect_alternatives_by_type = []<class Meta>(std::type_identity<Meta>, auto types) consteval {
	constexpr auto is_discriminated_type = util::fn<[]<class Type>(std::type_identity<Type> /*type*/) -> bool {
		return requires { variant_discriminator<Type>::value; };
	}>;
	constexpr auto [ discriminated, plain ] = util::pack_partition(types, is_discriminated_type);
	constexpr auto discriminator_of = [](auto subject) -> auto {
		return variant_discriminator<type_t<subject>>::value.discriminator;
	};
	constexpr auto group_of = [ = ](auto discriminator, auto discriminated_pack) consteval -> auto {
		constexpr auto [... subjects ] = discriminated_pack;
		constexpr auto member_of = [ = ](auto subject) consteval -> auto {
			if constexpr (discriminator_of(subject) == discriminator) {
				return util::type_pack{subject};
			} else {
				return util::type_pack{};
			}
		};
		return util::type_pack{(util::type_pack{discriminator} + ... + member_of(subjects))};
	};
	constexpr auto collect_discriminators = [ = ](auto discriminated_pack) consteval -> auto {
		constexpr auto [... subjects ] = discriminated_pack;
		constexpr auto [... discriminators ] = util::pack_unique(discriminator_of(subjects)...);
		return (util::type_pack{} + ... + group_of(discriminators, discriminated_pack));
	};
	return util::type_pack{plain, collect_discriminators(type_t<discriminated>{})};
};

// We assume that primitive & normal object types can negotiate amongst themselves by tag overload
// precedence
template <class Meta, class Variant, class Type>
struct accept_plain_covariant : accept<Meta, Type> {
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		constexpr auto operator()(auto tag, auto& visit, auto&& subject) const
			-> std::invoke_result_t<const accept_type&, decltype(covariant_tag{tag}), decltype(visit), decltype(subject)> {
			return util::invoke_as<accept_type>(*this, covariant_tag{tag}, visit, std::forward<decltype(subject)>(subject));
		}
};

// Compose plain covariants into one struct to allow the compiler do overload resolution amongst the
// tags
template <class Meta, class Variant, class... Types>
struct accept_plain_covariants : accept_plain_covariant<Meta, Variant, Types>... {
		constexpr explicit accept_plain_covariants(auto* transfer) :
				accept_plain_covariant<Meta, Variant, Types>{transfer}... {}
		using accept_target_type = Variant;
		using accept_plain_covariant<Meta, Variant, Types>::accept_plain_covariant::operator()...;

		// fallback for variants which include only one primary covariant type (utf16 string, double number)
		constexpr auto operator()(this const auto& self, auto tag, auto& visit, auto&& subject) -> decltype(auto)
			requires(
				!(... || std::invocable<const accept_plain_covariant<Meta, Variant, Types>&, decltype(tag), decltype(visit), decltype(subject)>) &&
				(... || std::invocable<const accept<Meta, Types>&, decltype(tag), decltype(visit), decltype(subject)>)
			) {
			constexpr auto [... accept_types ] = util::pack_filter(
				util::type_pack<accept<Meta, Types>...>{},
				util::fn<[]<class Accept>(std::type_identity<Accept> /*type*/) consteval -> bool {
					return std::invocable<const Accept&, decltype(tag), decltype(visit), decltype(subject)>;
				}>
			);
			static_assert(sizeof...(accept_types) == 1);
			using accept_type = type_t<accept_types...[ 0 ]>;
			return util::invoke_as<accept_type>(self, tag, visit, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto recursive) -> auto {
			return (util::type_pack{} + ... + accept_plain_covariant<Meta, Variant, Types>::types(recursive));
		}
};

// A discriminator inspects an object-like subject and may claim it, returning a callback which
// must accept the moved subject as a specific variant alternative. If the discriminator declines
// then the acceptor moves on to the next discriminator.
template <class Meta, class Variant, class Discriminator, class... Types>
struct accept_discriminator;

template <class Meta, class Variant, class Pack>
struct accept_discriminator_unpack;

template <class Meta, class Variant, class Pack>
using accept_discriminator_t = accept_discriminator_unpack<Meta, Variant, Pack>::type;

template <class Meta, class Variant, class Discriminator, class... Types>
struct accept_discriminator_unpack<Meta, Variant, util::type_pack<Discriminator, Types...>>
		: std::type_identity<accept_discriminator<Meta, Variant, Discriminator, Types...>> {};

// Delegate to `accept_plain_covariant` and `accept_discriminated_covariants`
template <class Meta, class Variant, class Types>
struct accept_covariants;

template <class Meta, class Variant, class... Types>
using accept_covariants_from_t = accept_covariants<Meta, Variant, type_t<collect_alternatives_by_type(type<Meta>, util::type_pack<Types...>{})>>;

template <class Meta, class Variant, class... Plain, class... Discriminators>
struct accept_covariants<Meta, Variant, util::type_pack<util::type_pack<Plain...>, util::type_pack<Discriminators...>>>
		: accept_value_from<accept_plain_covariants<Meta, Variant, Plain...>>,
			accept_discriminator_t<Meta, Variant, Discriminators>... {
	private:
		using accept_plain_type = accept_plain_covariants<Meta, Variant, Plain...>;
		template <class Type>
		using accept_discriminated_type = accept_discriminator_t<Meta, Variant, Type>;

	public:
		constexpr explicit accept_covariants(auto* transfer) :
				accept_value_from<accept_plain_type>{transfer},
				accept_discriminated_type<Discriminators>{transfer}... {}

		constexpr auto operator()(this const auto& self, auto tag, auto& visit, auto&& subject) -> Variant
			requires(
				std::invocable<const accept_plain_type&, decltype(tag), decltype(visit), decltype(subject)> ||
				(... || std::invocable<const accept_discriminated_type<Discriminators>&, std::type_identity<decltype(subject)>, decltype(tag), decltype(visit), decltype(subject)>)
			) {
			return self.select(util::type_pack<Discriminators...>{}, tag, visit, std::forward<decltype(subject)>(subject));
		}

		consteval static auto types(auto recursive) -> auto {
			constexpr auto plain_types = accept_plain_type::types(recursive);
			constexpr auto discriminated_types = (util::type_pack{} + ... + accept_discriminated_type<Discriminators>::types(recursive));
			return plain_types + discriminated_types;
		}

	private:
		// Probe the next discriminator for a callback which accepts the subject
		template <class Discriminator, class... Rest>
		constexpr auto select(this const auto& self, util::type_pack<Discriminator, Rest...> /*discriminators*/, auto tag, auto& visit, auto&& subject) -> Variant {
			using discriminator_type = accept_discriminated_type<Discriminator>;
			using identity_type = std::type_identity<decltype(subject)>;
			if constexpr (std::invocable<const discriminator_type&, identity_type, decltype(tag), decltype(visit), decltype(subject)>) {
				auto accept_subject = util::invoke_as<discriminator_type>(self, identity_type{}, tag, visit, subject);
				if (accept_subject) {
					return (*accept_subject)(std::forward<decltype(subject)>(subject));
				}
			}
			return self.select(util::type_pack<Rest...>{}, tag, visit, std::forward<decltype(subject)>(subject));
		}

		// No discriminator claimed the subject; fall back to plain covariance
		constexpr auto select(this const auto& self, util::type_pack<> /*discriminators*/, auto tag, auto& visit, auto&& subject) -> Variant {
			if constexpr (std::invocable<const accept_plain_type&, decltype(tag), decltype(visit), decltype(subject)>) {
				return util::invoke_as<accept_value_from<accept_plain_type>>(self, tag, visit, std::forward<decltype(subject)>(subject));
			} else {
				throw js::type_error{u"Invalid object type"};
			}
		}
};

// Unpack `std::variant` alternative types and pass forward to `accept_covariants`
template <class Meta, class... Types>
struct accept<Meta, std::variant<Types...>> : accept_covariants_from_t<Meta, std::variant<Types...>, Types...> {
		using accept_covariants_from_t<Meta, std::variant<Types...>, Types...>::accept_covariants_from_t;
};

} // namespace js
