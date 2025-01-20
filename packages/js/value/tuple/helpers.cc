module;
#include <type_traits>
#include <utility>
export module isolated_js.tuple.helpers;
import ivm.utility;

namespace js {

// Given the index, makes a pair corresponding to the selected type index and the type that precedes it.
template <std::size_t Index, class... Types>
struct param_pair;

template <std::size_t Index, class... Types>
using param_pair_t = typename param_pair<Index, Types...>::type;

template <std::size_t Index, class... Types>
struct param_pair
		: std::type_identity<std::pair<
				util::select_t<Index, Types...>,
				util::select_t<Index - 1, Types...>>> {};

template <class Type, class... Types>
struct param_pair<0, Type, Types...>
		: std::type_identity<std::pair<Type, std::type_identity<void>>> {};

// Makes a `std::tuple` of param pairs
template <class Sequence, class Types>
struct param_pair_sequence;

template <std::size_t... Index, class... Types>
struct param_pair_sequence<std::index_sequence<Index...>, std::tuple<Types...>>
		: std::type_identity<std::tuple<param_pair_t<Index, Types...>...>> {};

// Creates a tuple of param pairs from packed params.
// For example, `[int, char]` becomes `[(int, void), (char, int)]`
export template <class... Types>
using make_param_pair_sequence =
	param_pair_sequence<std::make_index_sequence<sizeof...(Types)>, std::tuple<Types...>>::type;

} // namespace js
