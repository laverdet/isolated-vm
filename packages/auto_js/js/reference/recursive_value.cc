module;
#include <utility>
export module auto_js:recursive_value;
import util;

namespace js {

// Simple value holder implementing inheritable constructors and dereference operators.
template <class Type>
class in_place_value_holder : public util::pointer_facade {
	public:
		using value_type = Type;

		constexpr in_place_value_holder(std::in_place_t /*tag*/, const Type& value)
			requires std::copy_constructible<value_type> :
				value_{value} {}
		constexpr in_place_value_holder(std::in_place_t /*tag*/, Type&& value)
			requires std::move_constructible<value_type> :
				value_{std::move(value)} {}
		explicit constexpr in_place_value_holder(std::in_place_t /*tag*/, auto&&... args)
			requires std::constructible_from<value_type, decltype(args)...> :
				value_{std::forward<decltype(args)>(args)...} {}

		[[nodiscard]] constexpr auto operator*() const -> const value_type& { return value_; }
		[[nodiscard]] constexpr auto operator*() && -> value_type&& { return std::move(value_); }

	private:
		value_type value_;
};

// Recursive value type used by `referential_value`. The reference types used by this value types
// are stored within the type and made available to acceptors.
export template <template <class> class Make>
class recursive_value : public in_place_value_holder<Make<recursive_value<Make>>> {
	private:
		using holder_type = in_place_value_holder<Make<recursive_value<Make>>>;

	public:
		using holder_type::holder_type;
		using typename holder_type::value_type;
		constexpr static auto is_recursive_type = true;
};

} // namespace js
