module;
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits.type_pack;
import :functional.function_constant;
import :utility;

namespace util {

// Capture a pack of types into a single type
export template <class... Types>
struct type_pack {
		using type = type_pack;
		type_pack() = default;

		template <class... Args>
		explicit consteval type_pack(Args... /*types*/)
			requires(sizeof...(Types) > 0 && (... && std::same_as<typename Args::type, Types>)) {}

		// Structured binding accessor
		// Explicit return type causes error:
		// "candidate template ignored: substitution failure [with Index = 0]: invalid index 0 for pack 'Types' of size 0"
		// ...even with `requires(Index < sizeof...(Types))` on the template and/or signature
		template <std::size_t Index>
		[[nodiscard]] consteval auto get() const /* -> std::type_identity<Types... [ Index ]> */ {
			// return {};
			return std::type_identity<Types... [ Index ]> {};
		}
};

// Prevent `type_pack{type_pack{}}` from flattening itself via copy construction
template <class... Types>
type_pack(type_pack<Types...>) -> type_pack<type_pack<Types...>>;

// Accept any `T::type` producing structure
template <class... Types>
type_pack(Types...) -> type_pack<typename Types::type...>;

// Concatenate multiple type_packs together
export constexpr auto pack_concat = util::overloaded{
	[]() consteval -> auto { return type_pack{}; },
	[](auto pack) consteval -> auto { return pack; },
	[](this auto pack_concat, auto left, auto right, auto&&... rest) consteval -> auto {
		const auto [... lefts ] = left;
		const auto [... rights ] = right;
		return pack_concat(type_pack{lefts..., rights...}, rest...);
	},
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

} // namespace util

namespace std {

// `util::type_pack` metaprogramming specializations
template <std::size_t Index, class... Types>
struct tuple_element<Index, util::type_pack<Types...>> {
		using type = std::type_identity<Types...[ Index ]>;
};

template <class... Types>
struct tuple_size<util::type_pack<Types...>> {
		constexpr static auto value = sizeof...(Types);
};

} // namespace std
