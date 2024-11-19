module;
#include <cstddef>
#include <tuple>
#include <utility>
#include <variant>
export module ivm.value:tuple.visit;
import :transfer;

namespace ivm::value {

template <class Meta, class First, class Second>
struct visit<Meta, std::pair<First, Second>> {
	public:
		visit() = default;
		constexpr visit(int dummy, const auto_visit auto& visit_) :
				first{dummy, visit_},
				second{dummy, visit_} {}

		visit<Meta, First> first;
		visit<Meta, Second> second;
};

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	public:
		visit() = default;
		constexpr visit(int dummy, const auto_visit auto& visit_) :
				visit_{visit<Meta, Types>{dummy, visit_}...} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& value, const auto& accept) const -> decltype(auto) {
			return std::get<Index>(visit_)(std::get<Index>(std::forward<decltype(value)>(value)), accept);
		}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(tuple_tag<sizeof...(Types)>{}, std::forward<decltype(value)>(value), *this);
		}

	private:
		std::tuple<visit<Meta, Types>...> visit_;
};

} // namespace ivm::value
