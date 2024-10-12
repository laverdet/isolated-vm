module;
#include <ranges>
#include <vector>
export module ivm.value:dictionary;
import ivm.utility;

namespace ivm::value {

export template <class Tag, class Key, class Value>
class dictionary {
	public:
		using value_type = std::pair<Key, Value>;
		using container_type = std::vector<value_type>;
		using const_iterator = container_type::const_iterator;
		using iterator = container_type::iterator;

		dictionary() = default;
		constexpr explicit dictionary(std::ranges::range auto&& range) :
				values{std::ranges::begin(range), std::ranges::end(range)} {}

		[[nodiscard]] constexpr auto begin(this auto&& self) { return self.values.begin(); }
		[[nodiscard]] constexpr auto end(this auto&& self) { return self.values.end(); }

	private:
		container_type values;
};

static_assert(std::ranges::range<dictionary<void, int, int>>);
static_assert(std::random_access_iterator<dictionary<void, int, int>::const_iterator>);

} // namespace ivm::value
