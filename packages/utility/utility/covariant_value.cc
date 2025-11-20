module;
#include <utility>
#include <variant>
export module util:utility.covariant_value;
import :utility.facade;
import :type_traits;

namespace util {

// Allows passing around virtual classes by value, by enumerating all derived types.
export template <class Type, class... Types>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class covariant_value : public pointer_facade {
	public:
		template <class Derived>
			requires(... || (type<Derived> == type<Types>))
		constexpr explicit covariant_value(Derived value) :
				value_{std::move(value)} {}

		constexpr auto operator*(this auto& self) -> auto& {
			using reference_type = util::apply_cvref_t<decltype(self), Type>;
			if (self.value_.index() == std::variant_npos) {
				std::unreachable();
			} else {
				return std::visit([](auto& value) -> reference_type { return value; }, self.value_);
			}
		}

	private:
		std::variant<Types...> value_;
};

} // namespace util
