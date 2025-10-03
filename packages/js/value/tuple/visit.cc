module;
#include <cstddef>
#include <tuple>
#include <utility>
export module isolated_js:tuple.visit;
import :transfer;
import ivm.utility;

namespace js {

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	private:
		using visitors_type = std::tuple<visit<Meta, std::decay_t<Types>>...>;

	public:
		constexpr explicit visit(auto* transfer) :
				visit_{[ & ]() constexpr -> visitors_type {
					const auto [... indices ] = util::sequence<std::tuple_size_v<visitors_type>>;
					return {util::elide(util::constructor<std::tuple_element_t<indices, visitors_type>>, transfer)...};
				}()} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& value, auto& accept) const -> decltype(auto) {
			return std::get<Index>(visit_)(std::get<Index>(std::forward<decltype(value)>(value)), accept);
		}

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, tuple_tag<sizeof...(Types)>{}, *this, std::forward<decltype(value)>(value));
		}

	private:
		visitors_type visit_;
};

} // namespace js
