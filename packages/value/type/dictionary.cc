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
		explicit dictionary(std::ranges::range auto&& range) :
				values{std::ranges::begin(range), std::ranges::end(range)} {}

		[[nodiscard]] auto begin() -> iterator { return values.begin(); }
		[[nodiscard]] auto begin() const -> const_iterator { return values.begin(); }
		[[nodiscard]] auto end() -> iterator { return values.end(); }
		[[nodiscard]] auto end() const -> const_iterator { return values.end(); }

	private:
		container_type values;
};

static_assert(std::ranges::range<dictionary<void, int, int>>);
static_assert(std::random_access_iterator<dictionary<void, int, int>::const_iterator>);

} // namespace ivm::value
