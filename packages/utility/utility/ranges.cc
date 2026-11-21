export module util:utility.ranges;
import std;

namespace util {

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

// Returns either a `std::tuple<size_type>` with the range's size, or `std::tuple<>` if it can't be
// determined in constant time.
export template <class Type>
constexpr auto maybe_range_size(const Type& range, auto... if_not) {
	if constexpr (std::ranges::sized_range<Type>) {
		return std::tuple{std::ranges::size(range)};
	} else {
		return std::tuple{if_not...};
	}
}

} // namespace util
