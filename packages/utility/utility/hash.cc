module;
#include <cstdint>
#include <ranges>
export module ivm.utility.hash;

namespace util {

// `constexpr` hash for property lookup
export constexpr auto djb2_hash(std::ranges::range auto&& view) -> uint32_t {
	uint32_t hash = 5'381;
	for (auto character : std::forward<decltype(view)>(view)) {
		hash = ((hash << 5U) + hash) + character;
	}
	return hash;
}

} // namespace util
