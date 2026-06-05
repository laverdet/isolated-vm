export module util:utility.sequence;
import std;

namespace util {

// Return a sequence of index constants
export template <std::size_t Size>
constexpr auto sequence = []() consteval -> auto {
	// With C++26 P2686 we can do constexpr decomposition. So instead of the `tuple` of
	// `integral_constants` it can be an array of `std::size_t`.
	// https://clang.llvm.org/cxx_status.html

	// std::array<std::size_t, Size> result{};
	// std::ranges::copy(std::ranges::views::iota(std::size_t{0}, Size), result.begin());
	// return result;

	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{std::integral_constant<std::size_t, Index>{}...};
	}(std::make_index_sequence<Size>());
}();

} // namespace util
