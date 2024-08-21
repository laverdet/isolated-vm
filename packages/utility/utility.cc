module;
#include <bits/functional_hash.h>
#include <ranges>
#include <utility>
export module ivm.utility:utility;

// https://en.cppreference.com/w/cpp/utility/variant/visit
export template <class... Visitors>
struct overloaded : Visitors... {
		using Visitors::operator()...;
};

// Some good thoughts here. It's strange that there isn't an easier way to transform an underlying
// range.
// https://brevzin.github.io/c++/2024/05/18/range-customization/
export constexpr auto into_range(std::ranges::range auto&& range) -> decltype(auto) {
	return range;
}

export constexpr auto into_range(auto&& range) -> decltype(auto)
	requires requires() {
		{ ~range };
	}
{
	return ~range;
}

// Explicitly move-constructs a new object from an existing rvalue reference. Used to immediately
// destroy a resource-consuming object after the result scope exists.
// Similar to `std::exchange` but for objects which are not move-assignable
export auto take(std::move_constructible auto&& value) {
	return std::remove_reference_t<decltype(value)>{std::forward<decltype(value)>(value)};
}
