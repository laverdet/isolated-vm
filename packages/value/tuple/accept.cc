module;
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include <variant>
export module ivm.value:tuple.accept;
import :tag;
import :transfer;
import ivm.utility;

namespace ivm::value {

template <class Meta, class First, class Second>
struct accept<Meta, std::pair<First, Second>> {
	public:
		explicit constexpr accept(const auto_visit auto& visit) :
				first{visit},
				second{visit} {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& accept) :
				first{dummy, visit, accept},
				second{dummy, visit, accept} {}

		accept<Meta, First> first;
		accept<Meta, Second> second;
};

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	public:
		explicit constexpr accept(const auto_visit auto& visit) :
				acceptors_{accept_next<Meta, Types>{visit}...} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept*/) :
				accept{visit} {}

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

} // namespace ivm::value
