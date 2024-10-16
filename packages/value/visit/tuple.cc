module;
#include <functional>
#include <iterator>
#include <tuple>
#include <variant>
export module ivm.value:tuple;
import :tag;
import :visit;

namespace ivm::value {

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	public:
		constexpr auto operator()(vector_tag /*tag*/, auto&& value) const -> std::tuple<Types...> {
			auto it = std::begin(value);
			auto end = std::end(value);
			auto next = [ & ](const auto& accept) constexpr -> decltype(auto) {
				if (it == end) {
					return accept(undefined_tag{}, std::monostate{});
				} else {
					return invoke_visit(*it++, accept);
				}
			};
			return std::invoke(
				[ & ]<size_t... Index>(std::index_sequence<Index...> /*indices*/) constexpr -> std::tuple<Types...> {
					return {next(std::get<Index>(acceptors_))...};
				},
				std::index_sequence_for<Types...>{}
			);
		}

	private:
		std::tuple<accept_next<Meta, Types>...> acceptors_;
};

} // namespace ivm::value
