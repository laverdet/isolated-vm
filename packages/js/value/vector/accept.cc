module;
#include <ranges>
#include <vector>
export module isolated_js.vector.accept;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

template <class Meta, class Type>
struct accept<Meta, std::vector<Type>> : accept_next<Meta, Type> {
		using accept_next<Meta, Type>::accept_next;

		constexpr auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> std::vector<Type> {
			// nb: This doesn't check for string keys, so like `Object.assign([ 1 ], { foo: 2 })` might
			// yield `[ 1, 2 ]`
			const accept_next<Meta, Type>& acceptor = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value).second, acceptor);
				});
			return {range.begin(), range.end()};
		}

		constexpr auto operator()(vector_tag /*tag*/, auto&& list, const auto& visit) const -> std::vector<Type> {
			const accept_next<Meta, Type>& acceptor = *this;
			auto range =
				std::forward<decltype(list)>(list) |
				std::views::transform([ & ](auto&& value) -> Type {
					return visit(std::forward<decltype(value)>(value), acceptor);
				});
			return {range.begin(), range.end()};
		}
};

} // namespace js
