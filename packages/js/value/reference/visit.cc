module;
#include <utility>
export module isolated_js:reference.visit;
import :recursive_value;
import :transfer;

namespace js {

// `recursive_value` visitor delegates to its underlying value visitor.
template <class Meta, template <class> class Make>
struct visit<Meta, recursive_value<Make>> : visit<Meta, typename recursive_value<Make>::value_type> {
		using visit_type = visit<Meta, typename recursive_value<Make>::value_type>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& subject, Accept& accept) const -> accept_target_t<Accept> {
			return util::invoke_as<visit_type>(*this, *std::forward<decltype(subject)>(subject), accept);
		}
};

} // namespace js
