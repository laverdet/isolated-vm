module;
#include <cstddef>
#include <tuple>
#include <utility>
export module isolated_js.tuple.visit;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;
using std::get;

namespace js {

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	public:
		constexpr explicit visit(auto* root) :
				visit_{util::make_tuple_in_place(
					[ & ] constexpr { return visit<Meta, std::decay_t<Types>>{root}; }...
				)} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& value, const auto& accept) const -> decltype(auto) {
			return get<Index>(visit_)(std::get<Index>(std::forward<decltype(value)>(value)), accept);
		}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(tuple_tag<sizeof...(Types)>{}, std::forward<decltype(value)>(value), *this);
		}

	private:
		std::tuple<visit<Meta, std::decay_t<Types>>...> visit_;
};

} // namespace js
