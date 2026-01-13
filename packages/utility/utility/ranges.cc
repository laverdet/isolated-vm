module;
#include <ranges>
#include <utility>
export module util:utility.ranges;

namespace util {

// Forward value category of a range to the iterated elements.
// Don't give it a temporary or it will dangle!
export constexpr auto forward_range(auto& range) -> auto& {
	return range;
}

export constexpr auto forward_range(auto&& range) {
	using value_type = decltype(*std::begin(std::forward<decltype(range)>(range)));
	if constexpr (std::is_reference_v<value_type>) {
		auto forward =
			range | std::views::transform([](auto& value) -> auto&& {
				return std::forward_like<decltype(range)>(value);
			});
		return forward;
	} else {
		return std::forward<decltype(range)>(range);
	}
}

// Some good thoughts here. It's strange that there isn't an easier way to transform an underlying
// range.
// https://brevzin.github.io/c++/2024/05/18/range-customization/
template <class Type>
concept meta_range = requires(Type range) { range.into_range(); };

export constexpr auto into_range(std::ranges::range auto&& range) -> auto&& {
	return std::forward<decltype(range)>(range);
}

export constexpr auto into_range(meta_range auto&& range) {
	return std::forward<decltype(range)>(range).into_range();
}

} // namespace util
