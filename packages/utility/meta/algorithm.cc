export module util:meta.algorithm;
import :functional;
import std;

namespace util {

// Makes a `util::type_pack<T...>` from `Something<T...>`
export constexpr auto make_type_pack = util::overloaded{
	// nb: Prevent `make_type_pack{type_pack_of{...}}` from turning into a mess of hidden types
	[]<class... Types>(type_pack_of<Types...> pack) consteval -> auto { return pack; },
	[]<template <class...> class Pack, class... Types>(std::type_identity<Pack<Types...>> /*type*/) consteval -> auto {
		return type_pack_of{type<Types>...};
	},
};

// Resolves a `Something<T...>` from `SomethingElse<T...>`
export template <template <class...> class Into>
constexpr auto spread_type_pack = util::overloaded{
	[]<class... Hidden>(type_pack_of<Hidden...> /*pack*/) consteval -> auto { return type<Into<typename Hidden::type...>>; },
	[]<template <class...> class Pack, class... Types>(std::type_identity<Pack<Types...>> /*type*/) consteval -> auto {
		return type<Into<Types...>>;
	},
};

// Apply transformation function to each type in the pack
export constexpr auto pack_transform = [](auto pack, auto unary) consteval -> auto {
	constexpr auto [... types ] = pack;
	return (type_pack{} + ... + make_type_pack(unary(types)));
};

// Return a new `type_pack` with types matching the predicate
// nb: `predicate` must be `util::fn<...>`
export constexpr auto pack_filter = [](auto pack, auto predicate) consteval -> auto {
	constexpr auto filter = [ = ](auto type) {
		if constexpr (predicate(type)) {
			return type_pack{type};
		} else {
			return type_pack{};
		}
	};
	return pack_transform(make_type_pack(pack), filter);
};

// Return a nested `type_pack` of two `tuple_pack`'s containing types filtered by the predicate.
export constexpr auto pack_partition = []<auto Predicate>(auto pack, function_constant<Predicate> predicate) consteval -> auto {
	// `std::not_fn()` doesn't work
	constexpr auto not_fn = util::fn<[](auto type) -> bool { return !Predicate(type); }>;
	const auto left = pack_filter(pack, predicate);
	const auto right = pack_filter(pack, not_fn);
	return type_pack_of{left, right};
};

// Return unique types from the pack
export constexpr auto pack_unique = util::overloaded{
	[] consteval -> auto { return type_pack{}; },
	[](auto... types) consteval -> auto {
		constexpr auto [... ii ] = util::sequence<sizeof...(types)>;
		return (... + [ & ] -> auto {
			constexpr auto subject = types...[ ii ];
			constexpr auto [... jj ] = util::sequence<ii>;
			if constexpr ((... || (subject == types...[ jj ]))) {
				return type_pack_of{};
			} else {
				return type_pack_of{subject};
			}
		}());
	},
};

} // namespace util
