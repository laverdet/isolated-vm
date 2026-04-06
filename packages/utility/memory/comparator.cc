export module util:memory.comparator;
import std;

namespace util {

// Range projection which extract underlying pointer of a type
export template <class Type>
struct pointer_projection {
		[[nodiscard]] constexpr auto operator()(const Type* pointer) const -> const Type* {
			return pointer;
		}

		template <class From>
		[[nodiscard]] constexpr auto operator()(const std::shared_ptr<From>& pointer) const -> const Type* {
			return pointer.get();
		}

		template <class From, class Deleter>
		[[nodiscard]] constexpr auto operator()(const std::unique_ptr<From, Deleter>& pointer) const -> const Type* {
			return pointer.get();
		}
};

// General pointer binary comparison
template <class Type, class Compare>
class pointer_compare : private pointer_projection<Type> {
	private:
		using pointer_projection<Type>::operator();

	public:
		using is_transparent = void;

		constexpr explicit pointer_compare(Compare compare = {}) :
				compare_{std::move(compare)} {}

		template <class Left, class Right>
		constexpr auto operator()(const Left& left, const Right& right) const -> bool {
			return compare_(operator()(left), operator()(right));
		}

	private:
		Compare compare_;
};

// Concrete pointer binary comparators
export template <class Type>
using pointer_equal_to = pointer_compare<Type, std::equal_to<>>;

export template <class Type>
using pointer_less = pointer_compare<Type, std::less<>>;

} // namespace util
