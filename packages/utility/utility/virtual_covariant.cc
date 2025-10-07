module;
#include <utility>
#include <variant>
export module ivm.utility:virtual_covariant;
import :facade;
import :type_traits.traits;
import :type_traits.type_of;

namespace util {

// Allows passing around virtual classes by value, by enumerating all derived types.
export template <class Type, class... Types>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class virtual_covariant : public pointer_facade {
	public:
		template <class Derived>
			requires(... || (type<Derived> == type<Types>))
		constexpr explicit virtual_covariant(Derived value) :
				value_{std::move(value)} {}

		constexpr auto operator*(this auto& self) -> auto& {
			using reference_type = util::apply_cv_ref_t<decltype(self), Type>;
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
