module;
#include <type_traits>
export module util:meta.algorithm;
import :functional;
import :utility;

namespace util {

// Makes a `util::type_pack<T...>` from `Something<T...>`
export constexpr auto make_type_pack = util::overloaded{
	// nb: Prevent `make_type_pack{type_pack_of{...}}` from turning into a mess of hidden types
	[]<class... Types>(type_pack_of<Types...> pack) consteval -> auto { return pack; },
	[]<template <class> class Pack, class... Types>(std::type_identity<Pack<Types...>> /*type*/) consteval -> auto {
		return type_pack_of{type<Types>...};
	},
};

// Concatenate 0..N `util::type_packs` together
export constexpr auto pack_concat = util::overloaded{
	[]() consteval -> auto { return type_pack{}; },
	[](auto&&... packs) consteval -> auto { return (... + make_type_pack(packs)); },
};

// Apply transformation function to each type in the pack
export constexpr auto pack_transform = [](auto pack, auto unary) consteval -> auto {
	const auto [... types ] = pack;
	return pack_concat(unary(types)...);
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
	return pack_transform(pack, filter);
};

// Return a nested `type_pack` of `tuple_pack`'s containing types filtered by the predicates
// nb: `predicates` must be `util::fn<...>`
export constexpr auto pack_partition = util::overloaded{
	[](auto pack) consteval -> auto { return type_pack{pack}; },
	[]<class Predicate>(this auto pack_partition, auto pack, Predicate predicate, auto... predicates) consteval -> auto {
		// `std::not_fn()` doesn't work
		constexpr auto not_fn = util::fn<[](auto type) -> bool { return !Predicate{}(type); }>;
		const auto left = pack_filter(pack, predicate);
		const auto right = pack_filter(pack, not_fn);
		return pack_concat(pack_partition(left), pack_partition(right, predicates...));
	},
};

// Return unique types from the pack
export constexpr auto pack_unique = util::overloaded{
	[]() consteval -> auto { return type_pack{}; },
	[](auto... types) consteval -> auto {
		constexpr auto transform = [ = ](auto index) {
			constexpr auto subject = types...[ index ];
			const auto [... indices ] = util::sequence<index>;
			if constexpr ((... || (subject == types...[ indices ]))) {
				return type_pack_of{};
			} else {
				return type_pack_of{subject};
			}
		};
		const auto [... indices ] = util::sequence<sizeof...(types)>;
		return (... + transform(indices));
	},
};

} // namespace util
