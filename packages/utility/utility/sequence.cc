export module util:utility.sequence;
import :utility.array;
import :utility.constant_wrapper;
import std;

namespace util {

// Return a sequence of `std::integral_constant`.
// Keep an eye on P1789 and then replace with `std::index_sequence_for`.
export template <std::size_t Size>
constexpr auto sequence_cw = [] consteval -> auto {
	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{util::cw<Index>...};
	}(std::make_index_sequence<Size>());
}();

// Return a sequence of constexpr indices
export template <std::size_t Size>
constexpr auto sequence = [] consteval -> auto {
#if defined(__clang__)
	return sequence_cw<Size>;
#else
	constexpr_array<std::size_t, Size> result{};
	std::ranges::copy(std::ranges::views::iota(std::size_t{0}, Size), result.begin());
	return result;
#endif
}();

} // namespace util
