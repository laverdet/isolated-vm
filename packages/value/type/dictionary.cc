module;
#include <ranges>
#include <vector>
export module ivm.value:dictionary;
import ivm.utility;

namespace ivm::value {

export template <class Tag, class Value>
class vector_of {
	public:
		using value_type = Value;
		using container_type = std::vector<value_type>;
		using const_iterator = container_type::const_iterator;
		using iterator = container_type::iterator;

		vector_of() = default;
		constexpr explicit vector_of(std::ranges::range auto&& range) :
				values{std::ranges::begin(range), std::ranges::end(range)} {}

		[[nodiscard]] constexpr auto begin(this auto&& self) { return self.values.begin(); }
		[[nodiscard]] constexpr auto end(this auto&& self) { return self.values.end(); }

	private:
		container_type values;
};

static_assert(std::ranges::range<vector_of<void, int>>);
static_assert(std::random_access_iterator<vector_of<void, int>::const_iterator>);

export template <class Tag, class Key, class Value>
using dictionary = vector_of<Tag, std::pair<Key, Value>>;

} // namespace ivm::value
