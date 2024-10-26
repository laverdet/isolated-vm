module;
#include <cstddef>
#include <tuple>
#include <utility>
#include <variant>
export module ivm.value:tuple.visit;
import :visit;

namespace ivm::value {

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	public:
		visit() = default;
		constexpr visit(int dummy, const auto_visit auto& visit_) :
				visit_{visit<Meta, Types>{dummy, visit_}...} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& value, const auto& accept) const -> decltype(auto) {
			return std::get<Index>(visit_)(std::forward<decltype(value)>(value), accept);
		}

	private:
		std::tuple<visit<Meta, Types>...> visit_;
};

} // namespace ivm::value
