module;
#include <memory>
#include <utility>
export module util:utility.facade;

namespace util {

/**
 * Facade class for arithmetic operations. You have implement `operator+=(different_type)` and unary
 * `operator+()` and the rest is handled automatically.
 */
export template <class difference_type_, class wide_size_type = difference_type_>
class arithmetic_facade {
	public:
		using difference_type = difference_type_;

		// Type -= difference_type
		auto operator-=(this auto& self, difference_type offset) -> auto& { return self += -offset; }

		// ++Type / --Type
		auto operator++(this auto& self) -> auto& { return self += 1; }
		auto operator--(this auto& self) -> auto& { return self -= 1; }

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
		friend auto operator+(difference_type left, const auto& right) {
			return right + left;
		}

		// Type - Type
		auto operator-(this auto&& self, decltype(self) right) -> difference_type {
			return static_cast<difference_type>(wide_size_type{+self} - wide_size_type{+right});
		}
};

/**
 * Implements `operator[]` in the context of `operator+()` and `operator*()`.
 */
export template <class difference_type>
class array_facade {
	public:
		auto operator[](this auto&& self, difference_type offset) -> decltype(auto) {
			return *(std::forward<decltype(self)>(self) + offset);
		}
};

/**
 * Implements `operator->()` in the context of `operator*()`.
 */
export class pointer_facade {
	public:
		constexpr auto operator->(this auto&& self) -> auto* {
			return std::addressof(*std::forward<decltype(self)>(self));
		}
};

/**
 * Implements the requirements of `std::random_access_iterator`.
 */
export template <class difference_type, class wide_size_type = difference_type>
class random_access_iterator_facade
		: public arithmetic_facade<difference_type, wide_size_type>,
			public array_facade<difference_type> {};

} // namespace util
