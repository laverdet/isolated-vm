module;
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.utility:virtual_covariant;
import :facade;
import :type_traits;

namespace util {

// Allows passing around virtual classes by value, by enumerating all derived types.
export template <class Type, class... Types>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class virtual_covariant : public pointer_facade {
	public:
		template <class Derived>
			requires(... || std::is_same_v<Types, Derived>)
		constexpr explicit virtual_covariant(Derived value) :
				value_{std::move(value)} {}

		constexpr auto operator->(this auto& self) -> auto* {
			// TODO: Fix `apply_cv_t` for ref types
			using result_type = std::remove_reference_t<util::apply_cv_t<std::remove_reference_t<decltype(self)>, Type>>;
			if (self.value_.index() == std::variant_npos) {
				std::unreachable();
			} else {
				return std::visit([](auto& value) -> result_type* { return std::addressof(value); }, self.value_);
			}
		}

	private:
		std::variant<Types...> value_;
};

} // namespace util
