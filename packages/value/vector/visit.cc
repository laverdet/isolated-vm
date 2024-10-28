module;
#include <vector>
export module ivm.value:vector.visit;
import :transfer;
import ivm.utility;

namespace ivm::value {

template <class Meta, class Type>
struct visit<Meta, std::vector<Type>> : visit<Meta, Type> {
		using visit<Meta, Type>::visit;

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit<Meta, Type>& visit = *this;
			return accept(vector_tag{}, std::forward<decltype(value)>(value), visit);
		}
};

} // namespace ivm::value
