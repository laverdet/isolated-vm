module;
#include <array>
#include <functional>
#include <ranges>
#include <vector>
export module isolated_js.vector.accept;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

// Simply wraps `accept_next`, but with the addition of the `get` overload. This may be the acceptor
// for a tuple which wants a `get`able.
template <class Meta, class Type>
struct accept_list_element : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;
};

// `std::array` can only accept a tuple of statically known size
template <class Meta, std::size_t Size, class Type>
struct accept<Meta, std::array<Type, Size>> : accept_list_element<Meta, Type> {
		using accept_list_element<Meta, Type>::accept_list_element;

		template <std::size_t VectorSize>
		constexpr auto operator()(vector_n_tag<VectorSize> /*tag*/, auto&& list, const auto& visit) const -> std::array<Type, Size> {
			const accept_list_element<Meta, Type>& acceptor = *this;
			auto iterator = std::forward<decltype(list)>(list).begin();
			return std::invoke(
				[ & ]<std::size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					return std::array<Type, Size>{invoke(std::integral_constant<std::size_t, Index>{})...};
				},
				[ & ]<std::size_t Index>(std::integral_constant<std::size_t, Index> /*index*/) constexpr {
					return visit(*iterator++, acceptor);
				},
				std::make_index_sequence<Size>{}
			);
		}

		template <std::size_t TupleSize>
		constexpr auto operator()(tuple_tag<TupleSize> /*tag*/, auto&& tuple, const auto& visit) const -> std::array<Type, Size> {
			const accept_list_element<Meta, Type>& acceptor = *this;
			return std::invoke(
				[ & ]<std::size_t... Index>(std::index_sequence<Index...> /*indices*/) constexpr {
					return std::array<Type, Size>{visit(std::integral_constant<std::size_t, Index>{}, std::forward<decltype(tuple)>(tuple), acceptor)...};
				},
				std::make_index_sequence<Size>{}
			);
		}
};

// `std::vector` can accept all kinds of lists
template <class Meta, class Type>
struct accept<Meta, std::vector<Type>> : accept_list_element<Meta, Type> {
		using accept_list_element<Meta, Type>::accept_list_element;

		constexpr auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> std::vector<Type> {
			// nb: This doesn't check for string keys, so like `Object.assign([ 1 ], { foo: 2 })` might
			// yield `[ 1, 2 ]`
			const accept_next<Meta, Type>& acceptor = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value).second, acceptor);
				});
			return {std::from_range, std::move(range)};
		}

		constexpr auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> std::vector<Type> {
			const accept_next<Meta, Type>& acceptor = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value), acceptor);
				});
			return {std::from_range, std::move(range)};
		}

		template <std::size_t Size>
		constexpr auto operator()(tuple_tag<Size> /*tag*/, auto&& tuple, const auto& visit) const -> std::vector<Type> {
			const accept_list_element<Meta, Type>& acceptor = *this;
			return std::invoke(
				[ & ]<std::size_t... Index>(std::index_sequence<Index...> /*indices*/) constexpr {
					std::vector<Type> result;
					result.reserve(Size);
					(result.emplace_back(visit(std::integral_constant<std::size_t, Index>{}, std::forward<decltype(tuple)>(tuple), acceptor)), ...);
					return result;
				},
				std::make_index_sequence<Size>{}
			);
		}
};

} // namespace js
