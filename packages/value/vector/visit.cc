module;
#include <span>
#include <vector>
export module ivm.value:vector.visit;
import :transfer;
import ivm.utility;

namespace ivm::value {

template <class Type>
struct transferee_subject<std::span<Type>> : std::type_identity<Type> {};

template <class Type>
struct transferee_subject<std::vector<Type>> : std::type_identity<Type> {};

template <class Meta, class Type>
struct visit<Meta, std::span<Type>> : visit<Meta, Type> {
		using visit<Meta, Type>::visit;

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, Type>& visit = *this;
			return accept(vector_tag{}, std::forward<decltype(value)>(value), visit);
		}
};

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, std::span<Type>> {
		using visit<Meta, std::span<Type>>::visit;

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, std::span<Type>>& visit = *this;
			return visit(std::span{std::forward<decltype(value)>(value)}, accept);
		}
};

} // namespace ivm::value
