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
		using visitors_type = std::tuple<visit<Meta, std::remove_cvref_t<Types>>...>;

	public:
		constexpr explicit visit(auto* transfer) :
				visit_{[ & ]() constexpr -> visitors_type {
					const auto [... indices ] = util::sequence<std::tuple_size_v<visitors_type>>;
					return {util::elide(util::constructor<std::tuple_element_t<indices, visitors_type>>, transfer)...};
				}()} {}

		template <size_t Index, class Accept>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return std::get<Index>(visit_)(std::get<Index>(std::forward<decltype(subject)>(subject)), accept);
		}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(tuple_tag<sizeof...(Types)>{}, *this, std::forward<decltype(subject)>(subject));
		}

	private:
		visitors_type visit_;
};

} // namespace js
