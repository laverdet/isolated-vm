module;
#include <cstddef>
#include <tuple>
#include <utility>
export module isolated_js.tuple.visit;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

template <class Meta, class... Types>
struct visit<Meta, std::tuple<Types...>> {
	private:
		using visitors_type = std::tuple<visit<Meta, std::decay_t<Types>>...>;

	public:
		constexpr explicit visit(auto* root) :
				visit_{[ & ]() constexpr {
					auto [... indices ] = util::make_sequence<std::tuple_size_v<visitors_type>>();
					return visitors_type{util::elide(util::constructor<std::tuple_element_t<indices, visitors_type>>, root)...};
				}()} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& value, const auto& accept) const -> decltype(auto) {
			return std::get<Index>(visit_)(std::get<Index>(std::forward<decltype(value)>(value)), accept);
		}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(tuple_tag<sizeof...(Types)>{}, std::forward<decltype(value)>(value), *this);
		}

	private:
		visitors_type visit_;
};

} // namespace js
