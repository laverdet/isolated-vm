module;
#include <functional>
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
#include <variant>
export module isolated_js.tuple.accept;
import isolated_js.tag;
import isolated_js.transfer;
import isolated_js.tuple.helpers;
import ivm.utility;

namespace js {

// When this parameter is accepted in a tuple, the *next* parameter will be a vector of the
// remaining arguments. It represents `function(...rest) {}`.
export struct rest {};

// Implements iterator increment & rest accept. `Type` is a `std::pair` of:
// [ element type, *previous* element type ]
template <class Meta, class Type>
struct accept_tuple_param;

// Normal parameter
template <class Meta, class Type, class Previous>
struct accept_tuple_param<Meta, std::pair<Type, Previous>> : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;

		constexpr auto operator()(auto& iterator, auto& end, const auto& visit) const -> decltype(auto) {
			const accept_next<Meta, Type>& acceptor = *this;
			if (iterator == end) {
				return acceptor(undefined_in_tag{}, std::monostate{});
			} else {
				return visit(*iterator++, acceptor);
			}
		}
};

// Rest placeholder
template <class Meta, class Previous>
struct accept_tuple_param<Meta, std::pair<rest, Previous>> {
		explicit constexpr accept_tuple_param(auto_heritage auto /*accept_heritage*/) {}
		constexpr auto operator()(auto& /*iterator*/, auto& /*end*/, const auto& /*visit*/) const -> rest {
			return rest{};
		}
};

// Rest parameter
template <class Meta, class Type>
struct accept_tuple_param<Meta, std::pair<Type, rest>> : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;

		constexpr auto operator()(auto& iterator, auto& end, const auto& visit) const -> decltype(auto) {
			const accept_next<Meta, Type>& acceptor = *this;
			return acceptor(vector_tag{}, std::ranges::subrange{iterator, end}, visit);
		}
};

// Unwrap tuple of pairs into acceptors
template <class Meta, class Type>
struct accept_tuple_param_acceptors;

template <class Meta, class Type>
using accept_tuple_param_acceptors_t = accept_tuple_param_acceptors<Meta, Type>::type;

template <class Meta, class... Pairs>
struct accept_tuple_param_acceptors<Meta, std::tuple<Pairs...>>
		: std::type_identity<std::tuple<accept_tuple_param<Meta, Pairs>...>> {};

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	public:
		explicit constexpr accept(auto_heritage auto accept_heritage) :
				acceptors_{
					std::invoke([ & ](std::type_identity<Types> /*type*/ = {}) constexpr { return accept_heritage; })...
				} {}

		constexpr auto operator()(vector_tag /*tag*/, auto&& value, const auto& visit) const -> std::tuple<Types...> {
			auto range = util::into_range(std::forward<decltype(value)>(value));
			auto it = std::begin(range);
			auto end = std::end(range);
			return std::invoke(
				[ & ]<size_t... Index>(std::index_sequence<Index...> /*indices*/) constexpr -> std::tuple<Types...> {
					return {std::get<Index>(acceptors_)(it, end, visit)...};
				},
				std::index_sequence_for<Types...>{}
			);
		}

	private:
		accept_tuple_param_acceptors_t<Meta, make_param_pair_sequence<Types...>> acceptors_;
};

} // namespace js
