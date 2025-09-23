module;
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
#include <variant>
export module isolated_js.tuple.accept;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// When this parameter is accepted in a tuple, the *next* parameter will be a vector of the
// remaining arguments. It represents `function(...rest) {}`.
export struct rest {};

// Normal parameter
template <class Meta, class Type>
struct accept_tuple_param : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;

		constexpr auto range(auto& iterator, auto& end, const auto& visit) const -> decltype(auto) {
			if (iterator == end) {
				return (*this)(undefined_in_tag{}, std::monostate{});
			} else {
				return visit(*iterator++, *this);
			}
		}
};

// Rest placeholder
struct accept_tuple_rest_placeholder {
		explicit constexpr accept_tuple_rest_placeholder(auto* /*previous*/) {}
		constexpr auto range(auto& /*iterator*/, auto& /*end*/, const auto& /*visit*/) const -> rest {
			return rest{};
		}
};

// Rest parameter
template <class Meta, class Type>
struct accept_tuple_rest_spread : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;

		constexpr auto range(auto& iterator, auto& end, const auto& visit) const -> decltype(auto) {
			return (*this)(vector_tag{}, std::ranges::subrange{iterator, end}, visit);
		}
};

// Generates the `std::tuple<...>` type containing nested acceptors.
constexpr auto make_tuple_acceptor_types =
	[]<class Wrap, class Visit, class Accept, class Subject, class... Types>(
		std::type_identity<transferee_meta<Wrap, Visit, Subject, Accept, std::tuple<Types...>>> /*meta*/,
		std::type_identity<std::tuple<Types...>> /*types*/
	) consteval {
		constexpr auto size = sizeof...(Types);
		constexpr auto rest_param = []() consteval {
			if constexpr (size >= 2) {
				return std::is_same_v<Types...[ size - 2 ], rest> ? size - 1 : size;
			} else {
				return size;
			}
		}();
		auto [... indices ] = util::make_sequence<size>();
		auto [... acceptor_types ] = util::parameter_pack{
			[](auto index) consteval {
				using type_name = Types...[ index ];
				using next_meta = transferee_meta<Wrap, Visit, Subject, Accept, type_name>;
				if constexpr (std::is_same_v<type_name, rest>) {
					static_assert(index + 1 == rest_param, "`rest` must be second-to-last parameter in a tuple");
					return util::type<accept_tuple_rest_placeholder>;
				} else if constexpr (index == rest_param) {
					return util::type<accept_tuple_rest_spread<next_meta, type_name>>;
				} else {
					return util::type<accept_tuple_param<next_meta, type_name>>;
				}
			}(indices)...,
		};
		return util::type<std::tuple<util::meta_type_t<acceptor_types>...>>;
	};

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	private:
		using value_type = std::tuple<Types...>;
		using acceptors_type = util::meta_type_t<make_tuple_acceptor_types(util::type<Meta>, util::type<value_type>)>;

	public:
		explicit constexpr accept(auto* previous) :
				accept_{[ & ]() constexpr {
					auto [... indices ] = util::make_sequence<std::tuple_size_v<acceptors_type>>();
					return acceptors_type{util::elide(util::constructor<std::tuple_element_t<indices, acceptors_type>>, previous)...};
				}()} {}

		constexpr auto operator()(vector_tag /*tag*/, auto&& value, const auto& visit) const -> value_type {
			auto [... indices ] = util::make_sequence<sizeof...(Types)>();
			auto range = util::into_range(std::forward<decltype(value)>(value));
			auto it = std::begin(range);
			auto end = std::end(range);
			return {std::get<indices>(accept_).range(it, end, visit)...};
		}

	private:
		acceptors_type accept_;
};

} // namespace js
