module;
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

// `boost::noncopyable` actually prevents moving too
export class non_copyable {
	public:
		non_copyable() = default;
		non_copyable(const non_copyable&) = delete;
		non_copyable(non_copyable&&) = default;
		~non_copyable() = default;
		auto operator=(const non_copyable&) -> non_copyable& = delete;
		auto operator=(non_copyable&&) -> non_copyable& = default;
};

export class non_moveable {
	public:
		non_moveable() = default;
		non_moveable(const non_moveable&) = delete;
		non_moveable(non_moveable&&) = delete;
		~non_moveable() = default;
		auto operator=(const non_moveable&) -> non_moveable& = delete;
		auto operator=(non_moveable&&) -> non_moveable& = delete;
};

// https://en.cppreference.com/w/cpp/experimental/scope_exit
export template <class Invoke>
class scope_exit : non_copyable {
	public:
		explicit scope_exit(Invoke&& invoke) :
				invoke_{std::forward<Invoke>(invoke)} {}
		scope_exit(const scope_exit&) = delete;
		~scope_exit() { invoke_(); }
		auto operator=(const scope_exit&) -> scope_exit& = delete;

	private:
		Invoke invoke_;
};
