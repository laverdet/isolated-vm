module;
#include <type_traits>
#include <utility>
export module isolated_js:reference.accept;
import :recursive_value;
import :transfer;
import ivm.utility;

namespace js {

// `recursive_value` acceptor delegates to its underlying value acceptor.
template <class Meta, template <class> class Make>
struct accept<Meta, recursive_value<Make>> : accept<Meta, typename recursive_value<Make>::value_type> {
		using value_type = recursive_value<Make>;
		using accept_type = accept<Meta, typename value_type::value_type>;
		using accept_type::accept_type;

		constexpr auto operator()(this auto& self, auto tag, const auto& visit, auto&& subject) -> value_type
			requires std::is_invocable_v<accept_type&, decltype(tag), decltype(visit), decltype(subject)> {
			return value_type(util::invoke_as<accept_type>(self, tag, visit, std::forward<decltype(subject)>(subject)));
		}
};

} // namespace js
