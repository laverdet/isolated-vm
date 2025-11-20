module;
#include <iterator>
#include <ranges>
#include <tuple>
#include <utility>
#include <variant>
export module auto_js:tuple.accept;
import :transfer;
import util;

namespace js {

// When this parameter is accepted in a tuple, the *next* parameter will be a vector of the
// remaining arguments. It represents `function(...rest) {}`.
export struct rest {};

// Normal parameter
template <class Meta, class Type>
struct accept_tuple_param : accept_value<Meta, Type> {
		using accept_type = accept_value<Meta, Type>;
		constexpr accept_tuple_param() : accept_type{this} {}

		constexpr auto visit_and_advance(auto& visit, auto& iterator, auto& end) const -> accept_target_t<accept_type> {
			if (iterator == end) {
				return (*this)(undefined_in_tag{}, visit, std::monostate{});
			} else {
				return visit(*iterator++, *this);
			}
		}
};

// Rest placeholder
struct accept_tuple_rest_placeholder {
		constexpr auto visit_and_advance(auto& /*iterator*/, auto& /*end*/, const visit_holder /*visit*/) const -> rest {
			return {};
		}
};

// Rest parameter
template <class Meta, class Type>
struct accept_tuple_rest_spread : accept_value<Meta, Type> {
		using accept_type = accept_value<Meta, Type>;
		constexpr accept_tuple_rest_spread() : accept_type{this} {}

		constexpr auto visit_and_advance(auto& visit, auto& iterator, auto& end) const -> accept_target_t<accept_type> {
			return (*this)(vector_tag{}, std::ranges::subrange{iterator, end}, visit);
		}
};

// Generates the `std::tuple<...>` type containing nested acceptors.
template <class Meta, class... Types>
consteval auto make_tuple_acceptor_types() {
	constexpr auto size = sizeof...(Types);
	constexpr auto rest_param = []() consteval -> size_t {
		if constexpr (size >= 2) {
			return type<Types...[ size - 2 ]> == type<rest> ? size - 1 : size;
		} else {
			return size;
		}
	}();
	const auto [... indices ] = util::sequence<size>;
	const auto [... acceptor_types ] = util::type_pack{
		[](auto index) consteval {
			using type_name = Types...[ index ];
			if constexpr (type<type_name> == type<rest>) {
				static_assert(index + 1 == rest_param, "`rest` must be second-to-last parameter in a tuple");
				return type<accept_tuple_rest_placeholder>;
			} else if constexpr (index == rest_param) {
				return type<accept_tuple_rest_spread<Meta, type_name>>;
			} else {
				return type<accept_tuple_param<Meta, type_name>>;
			}
		}(indices)...,
	};
	return type<std::tuple<type_t<acceptor_types>...>>;
};

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	private:
		using value_type = std::tuple<Types...>;
		using acceptors_type = type_t<make_tuple_acceptor_types<Meta, Types...>()>;

	public:
		explicit constexpr accept(auto* /*transfer*/) :
				accept_{[ & ]() constexpr -> acceptors_type {
					const auto [... indices ] = util::sequence<std::tuple_size_v<acceptors_type>>;
					return {util::elide(util::constructor<std::tuple_element_t<indices, acceptors_type>>)...};
				}()} {}

		constexpr auto operator()(vector_tag /*tag*/, auto& visit, auto&& subject) const -> value_type {
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			auto&& forward_range = util::forward_range(std::forward<decltype(range)>(range));
			auto it = std::begin(forward_range);
			auto end = std::end(forward_range);
			const auto& [... accept_n ] = accept_;
			return {accept_n.visit_and_advance(visit, it, end)...};
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

		constexpr auto operator()(vector_tag /*tag*/, auto& visit, auto&& subject) const -> value_type {
			return {rest{}, util::invoke_as<accept_type>(*this, vector_tag{}, visit, std::forward<decltype(subject)>(subject))};
		}
};

} // namespace js
