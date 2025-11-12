module;
#include <ranges>
export module ivm.utility:utility.ranges;

namespace util {

// Some good thoughts here. It's strange that there isn't an easier way to transform an underlying
// range.
// https://brevzin.github.io/c++/2024/05/18/range-customization/
export constexpr auto into_range(std::ranges::range auto&& range) -> auto&& {
	return std::forward<decltype(range)>(range);
}

export constexpr auto into_range(auto&& range)
	requires requires { range.into_range(); } {
	return std::forward<decltype(range)>(range).into_range();
}

} // namespace util
