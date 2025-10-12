module;
#include <array>
#include <ranges>
#include <vector>
export module isolated_js:vector.accept;
import :transfer;
import ivm.utility;

namespace js {

template <class Type, size_t Size>
struct accept_target_for<std::array<Type, Size>> : accept_target_for<Type> {};

template <class Type>
struct accept_target_for<std::vector<Type>> : accept_target_for<Type> {};

// `std::array` can only accept a tuple of statically known size
template <class Meta, std::size_t Size, class Type>
struct accept<Meta, std::array<Type, Size>> : accept_value<Meta, Type> {
		using accept_type = accept_value<Meta, Type>;
		using accept_type::accept_type;

		template <std::size_t VectorSize>
		constexpr auto operator()(vector_n_tag<VectorSize> /*tag*/, const auto& visit, auto&& list) -> std::array<Type, Size> {
			accept_type& accept = *this;
			auto iterator = std::forward<decltype(list)>(list).begin();
			const auto [... indices ] = util::sequence<Size>;
			return std::array<Type, Size>{
				// nb: Comma operator trick
				(util::unused(indices), visit(*iterator++, accept))...
			};
		}

		template <std::size_t TupleSize>
		constexpr auto operator()(tuple_tag<TupleSize> /*tag*/, const auto& visit, auto&& tuple) -> std::array<Type, Size> {
			accept_type& accept = *this;
			const auto [... indices ] = util::sequence<TupleSize>;
			return std::array<Type, Size>{
				visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(tuple)>(tuple), accept)...,
			};
		}
};

// `std::vector` can accept all kinds of lists
template <class Meta, class Type>
struct accept<Meta, std::vector<Type>> : accept_value<Meta, Type> {
		using accept_type = accept_value<Meta, Type>;
		using accept_type::accept_type;

		constexpr auto operator()(list_tag /*tag*/, const auto& visit, auto&& list) -> std::vector<Type> {
			// nb: This doesn't check for string keys, so like `Object.assign([ 1 ], { foo: 2 })` might
			// yield `[ 1, 2 ]`
			accept_type& accept = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value).second, accept);
				});
			return {std::from_range, std::move(range)};
		}

		constexpr auto operator()(vector_tag /*tag*/, const auto& visit, auto&& list) -> std::vector<Type> {
			accept_type& accept = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value), accept);
				});
			return {std::from_range, std::move(range)};
		}

		template <std::size_t Size>
		constexpr auto operator()(tuple_tag<Size> /*tag*/, const auto& visit, auto&& tuple) -> std::vector<Type> {
			accept_type& accept = *this;
			std::vector<Type> result;
			result.reserve(Size);
			const auto [... indices ] = util::sequence<Size>;
			(..., result.emplace_back(visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(tuple)>(tuple), accept)));
			return result;
		}
};

} // namespace js
