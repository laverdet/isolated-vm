module;
#include <functional>
#include <iterator>
#include <tuple>
#include <utility>
#include <variant>
export module ivm.value:tuple.accept;
import :tag;
import :visit;

namespace ivm::value {

template <class Meta, class First, class Second>
struct accept<Meta, std::pair<First, Second>> {
	public:
		constexpr accept(const auto_visit auto& visit) :
				first{visit},
				second{visit} {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& accept) :
				first{dummy, visit, accept},
				second{dummy, visit, accept} {}

	public:
		accept<Meta, First> first;
		accept<Meta, Second> second;
};

// Accepting a `std::tuple` unfolds from a visited vector
template <class Meta, class... Types>
struct accept<Meta, std::tuple<Types...>> {
	public:
		constexpr accept(const auto_visit auto& visit) :
				acceptors_{accept_next<Meta, Types>{visit}...} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept*/) :
				accept{visit} {}

		template <size_t Index>
		constexpr auto operator()(std::integral_constant<size_t, Index> /*index*/, auto_tag auto tag, auto&& value, auto&&... rest) const -> decltype(auto) {
			return std::get<Index>(acceptors_)(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}

		constexpr auto operator()(vector_tag /*tag*/, auto&& value, const auto& visit) const -> std::tuple<Types...> {
			auto it = std::begin(value);
			auto end = std::end(value);
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
