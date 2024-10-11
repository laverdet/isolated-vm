module;
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
		constexpr auto operator()(vector_tag /*tag*/, auto&& value) const -> std::tuple<Types...> {
			auto it = std::begin(value);
			auto end = std::end(value);
			auto next = [ & ](auto accept) -> decltype(auto) {
				if (it == end) {
					return accept(undefined_tag{}, std::monostate{});
				} else {
					return invoke_visit(*it++, std::move(accept));
				}
			};
			return {next(make_accept<Types>(*this))...};
		}
};

} // namespace ivm::value
