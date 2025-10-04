module;
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
#include <variant>
export module isolated_js:tuple.accept;
import :transfer;
import ivm.utility;

namespace js {

// When this parameter is accepted in a tuple, the *next* parameter will be a vector of the
// remaining arguments. It represents `function(...rest) {}`.
export struct rest {};

// Normal parameter
template <class Meta, class Type>
struct accept_tuple_param : accept_next<Meta, Type> {
		constexpr accept_tuple_param() : accept_next<Meta, Type>{this} {}

		constexpr auto range(const auto& visit, auto& iterator, auto& end) -> decltype(auto) {
			if (iterator == end) {
				return (*this)(undefined_in_tag{}, visit, std::monostate{});
			} else {
				return visit(*iterator++, *this);
			}
		}
};

// Rest placeholder
struct accept_tuple_rest_placeholder {
		constexpr auto range(auto& /*iterator*/, auto& /*end*/, const visit_holder /*visit*/) const -> rest {
			return {};
		}
};

// Rest parameter
template <class Meta, class Type>
struct accept_tuple_rest_spread : accept_next<Meta, Type> {
		constexpr accept_tuple_rest_spread() : accept_next<Meta, Type>{this} {}

		constexpr auto range(const auto& visit, auto& iterator, auto& end) -> decltype(auto) {
			return (*this)(vector_tag{}, std::ranges::subrange{iterator, end}, visit);
		}
};

// Generates the `std::tuple<...>` type containing nested acceptors.
template <class Meta, class... Types>
consteval auto make_tuple_acceptor_types() {
	constexpr auto size = sizeof...(Types);
	constexpr auto rest_param = []() consteval {
		if constexpr (size >= 2) {
			return std::is_same_v<Types...[ size - 2 ], rest> ? size - 1 : size;
		} else {
			return size;
		}
	}();
	const auto [... indices ] = util::sequence<size>;
	const auto [... acceptor_types ] = util::parameter_pack{
		[](auto index) consteval {
			using type_name = Types...[ index ];
			if constexpr (std::is_same_v<type_name, rest>) {
				static_assert(index + 1 == rest_param, "`rest` must be second-to-last parameter in a tuple");
				return util::type<accept_tuple_rest_placeholder>;
			} else if constexpr (index == rest_param) {
				return util::type<accept_tuple_rest_spread<Meta, type_name>>;
			} else {
				return util::type<accept_tuple_param<Meta, type_name>>;
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
		using acceptors_type = util::meta_type_t<make_tuple_acceptor_types<Meta, Types...>()>;

	public:
		explicit constexpr accept(auto* /*transfer*/) :
				accept_{[ & ]() constexpr -> acceptors_type {
					const auto [... indices ] = util::sequence<std::tuple_size_v<acceptors_type>>;
					return {util::elide(util::constructor<std::tuple_element_t<indices, acceptors_type>>)...};
				}()} {}

		constexpr auto operator()(vector_tag /*tag*/, const auto& visit, auto&& value) -> value_type {
			const auto [... indices ] = util::sequence<sizeof...(Types)>;
			auto range = util::into_range(std::forward<decltype(value)>(value));
			auto it = std::begin(range);
			auto end = std::end(range);
			return {std::get<indices>(accept_).range(visit, it, end)...};
		}

	private:
		acceptors_type accept_;
};

// In the case of accepting only a rest parameter, the accept target can be remapped.
template <class Type>
struct accept_target_for<std::tuple<rest, Type>> : accept_target_for<Type> {};

template <class Meta, class Type>
struct accept<Meta, std::tuple<rest, Type>> : accept<Meta, Type> {
		using value_type = std::tuple<rest, Type>;
		using accept_type = accept<Meta, Type>;
		using accept_type::accept_type;

		constexpr auto operator()(vector_tag /*tag*/, const auto& visit, auto&& value) -> value_type {
			return {rest{}, accept_type::operator()(vector_tag{}, visit, std::forward<decltype(value)>(value))};
		}
};

} // namespace js
