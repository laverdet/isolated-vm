module;
#include <memory>
#include <type_traits>
#include <utility>
export module util:utility.facade;

namespace util {

// Internal addition facade
template <class difference_type_>
class addition_facade {
	public:
		using difference_type = difference_type_;

		// ++Type
		auto operator++(this auto& self) -> auto& { return self += 1; }

		// Type++
		auto operator++(this auto& self, int) {
			auto result = self;
			++self;
			return result;
		}

		// Type + difference_type
		auto operator+(this const auto& self, difference_type offset) {
			auto result = self;
			return result += offset;
		}

		// difference_type + Type
		friend auto operator+(difference_type left, const auto& right) {
			return right + left;
		}
};

// Internal subtraction facade
template <class difference_type_, class wide_size_type = difference_type_>
class subtraction_facade {
	public:
		auto operator--() = delete;
		auto operator-() = delete;
		auto operator-=(auto) = delete;
};

template <class difference_type_, class wide_size_type>
	requires std::is_signed_v<difference_type_>
class subtraction_facade<difference_type_, wide_size_type> {
	public:
		using difference_type = difference_type_;

		// Type -= difference_type
		auto operator-=(this auto& self, difference_type offset) -> auto& { return self += -offset; }

		// --Type
		auto operator--(this auto& self) -> auto& { return self -= 1; }

		// Type--
		auto operator--(this auto& self, int) {
			auto result = self;
			--self;
			return result;
		}

		// Type - difference_type
		auto operator-(this const auto& self, difference_type offset) {
			auto result = self;
			return result -= offset;
		}

		// Type - Type
		auto operator-(this const auto& self, decltype(self) right) -> difference_type {
			return static_cast<difference_type>(wide_size_type{+self} - wide_size_type{+right});
		}
};

/**
 * Facade class for numeric arithmetic operations. You have to implement
 * `operator+=(difference_type)` and unary `operator+()` (for subtraction) and the rest is handled
 * automatically.
 */
export template <class difference_type_, class wide_size_type = difference_type_>
class arithmetic_facade
		: public addition_facade<difference_type_>,
			public subtraction_facade<difference_type_, wide_size_type> {
	public:
		using difference_type = difference_type_;

		using addition_facade<difference_type>::operator+;
		using addition_facade<difference_type>::operator++;

		using subtraction_facade<difference_type, wide_size_type>::operator-;
		using subtraction_facade<difference_type, wide_size_type>::operator--;
		using subtraction_facade<difference_type, wide_size_type>::operator-=;
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
