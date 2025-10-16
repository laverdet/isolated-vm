module;
#include <utility>
export module isolated_js:reference.accept;
import :recursive_value;
import :transfer;
import ivm.utility;

namespace js {

// `recursive_value` acceptor delegates to its underlying value acceptor.
template <class Meta, template <class> class Make>
struct accept<Meta, recursive_value<Make>> : accept<Meta, typename recursive_value<Make>::value_type> {
	private:
		using value_type = recursive_value<Make>;
		using underlying_value_type = value_type::value_type;

	public:
		using accept_type = accept<Meta, underlying_value_type>;
		using accept_type::accept_type;

		using accept_type::operator();
		constexpr auto operator()(underlying_value_type&& target) -> value_type {
			return value_type{std::in_place, std::move(target)};
		}
};

} // namespace js
