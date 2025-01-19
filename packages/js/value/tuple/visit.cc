module;
#include <cstddef>
#include <tuple>
#include <utility>
export module isolated_js.tuple.visit;
import isolated_js.tag;
import isolated_js.transfer;

namespace js {

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	public:
		visit() = default;
		constexpr visit(int dummy, const visit_root<Meta>& visit_) :
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

} // namespace js
