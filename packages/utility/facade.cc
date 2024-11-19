module;
#include <utility>
export module ivm.utility:facade;

namespace ivm::util {

/**
 * Facade class for arithmetic operations. You have implement `operator+=(different_type)` and unary
 * `operator+()` and the rest is handled automatically.
 */
export template <class Type, class difference_type_, class wide_size_type = difference_type_>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
class arithmetic_facade {
	public:
		using difference_type = difference_type_;

		// Type -= difference_type
		auto operator-=(this auto& self, difference_type offset) -> decltype(auto) { return self += -offset; }

		// ++Type / --Type
		auto operator++(this auto& self) -> decltype(auto) { return self += 1; }
		auto operator--(this auto& self) -> decltype(auto) { return self -= 1; }

		// Type++ / Type--
		auto operator++(this auto& self, int) {
			auto result = self;
			++self;
			return result;
		}
		auto operator--(this auto& self, int) {
			auto result = self;
			--self;
			return result;
		}

		// Type +/- difference_type
		auto operator+(this const auto& self, difference_type offset) {
			auto result = self;
			return result += offset;
		}
		auto operator-(this const auto& self, difference_type offset) {
			auto result = self;
			return result -= offset;
		}

		// difference_type + Type
		friend auto operator+(difference_type left, const Type& right) {
			return right + left;
		}

		// Type - Type
		auto operator-(this auto&& self, decltype(self) right) {
			return static_cast<difference_type>(wide_size_type{+self} - wide_size_type{+right});
		}
};

/**
 * Implements `operator[]` in the context of `operator+()` and `operator*()`.
 */
export template <class Type, class difference_type>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
class array_facade {
	public:
		auto operator[](this auto&& self, difference_type offset) -> decltype(auto) {
			return *(std::forward<decltype(self)>(self) + offset);
		}
};

/**
 * Implements `operator->()` in the context of `operator*()`.
 */
export template <class Type>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
class pointer_facade {
	public:
		auto operator->(this auto& self) -> auto* { return &*self; }
};

/**
 * Implements the requirements of `std::random_access_iterator`.
 */
export template <class Type, class difference_type, class wide_size_type = difference_type>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
class random_access_iterator_facade
		: public arithmetic_facade<Type, difference_type, wide_size_type>,
			public array_facade<Type, difference_type> {};

} // namespace ivm::util
