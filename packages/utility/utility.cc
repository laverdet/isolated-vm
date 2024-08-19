module;
#include <utility>
export module ivm.utility:utility;

// https://en.cppreference.com/w/cpp/utility/variant/visit
export template <class... Visitors>
struct overloaded : Visitors... {
		using Visitors::operator()...;
};

// Explicitly move-constructs a new object from an existing rvalue reference. Used to immediately
// destroy a resource-consuming object after the result scope exists.
// Similar to `std::exchange` but for objects which are not move-assignable
export auto take(std::move_constructible auto&& value) {
	return std::remove_reference_t<decltype(value)>{std::forward<decltype(value)>(value)};
}
