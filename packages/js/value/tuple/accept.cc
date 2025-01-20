module;
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include <variant>
export module isolated_js.tuple.accept;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	public:
		explicit constexpr accept(auto_heritage auto accept_heritage) :
				acceptors_{util::make_tuple_in_place(
					[ & ] constexpr { return accept_next<Meta, Types>{accept_heritage}; }...
				)} {}

		constexpr auto operator()(vector_tag /*tag*/, auto&& value, const auto& visit) const -> std::tuple<Types...> {
			auto range = util::into_range(std::forward<decltype(value)>(value));
			auto it = std::begin(range);
			auto end = std::end(range);
			return std::invoke(
				[ & ]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr -> std::tuple<Types...> {
					return {invoke(std::get<Index>(acceptors_))...};
				},
				[ & ](const auto_accept auto& accept) constexpr -> decltype(auto) {
					if (it == end) {
						return accept(undefined_tag{}, std::monostate{});
					} else {
						return visit(*it++, accept);
					}
				},
				std::index_sequence_for<Types...>{}
			);
		}

	private:
		std::tuple<accept_next<Meta, Types>...> acceptors_;
};

} // namespace js
