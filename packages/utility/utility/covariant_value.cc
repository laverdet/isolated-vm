export module util:utility.covariant_value;
import :type_traits;
import :utility.facade;
import std;

namespace util {

// Allows passing around virtual classes by value, by enumerating all derived types.
export template <class Type, class... Types>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class covariant_value : public pointer_facade {
	public:
		constexpr covariant_value() : covariant_value{std::type_identity<Types...[ 0 ]>{}} {}

		template <class Derived>
			requires(... || (type<Derived> == type<Types>))
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr covariant_value(std::type_identity<Derived>, auto&&... args) :
				value_{std::in_place_type<Derived>, std::forward<decltype(args)>(args)...} {}

		constexpr auto operator*(this auto& self) noexcept -> auto& {
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
