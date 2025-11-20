module;
#include <ranges>
#include <vector>
export module auto_js:dictionary.vector_of;
import :tag;

namespace js {

// `vector_of` is just a vector with an associated js tag
export template <class Tag, class Value>
class vector_of : public std::vector<Value> {
	public:
		vector_of() = default;

		consteval explicit vector_of(std::in_place_t /*in_place*/, auto&&... args) {
			this->reserve(sizeof...(args));
			(..., this->emplace_back(std::forward<decltype(args)>(args)));
		}

		constexpr explicit vector_of(std::from_range_t from_range, std::ranges::range auto&& range) :
				std::vector<Value>{from_range, std::forward<decltype(range)>(range)} {}

		// For testing / assertions
		[[nodiscard]] consteval auto operator==(const vector_of& right) const -> bool {
			const std::vector<Value>& left = *this;
			return left == right;
		}
};

template <class Key, class Value>
vector_of(std::in_place_t, std::pair<Key, Value>, auto&&...) -> vector_of<dictionary_tag, std::pair<Key, Value>>;

static_assert(std::ranges::range<vector_of<void, int>>);
static_assert(std::random_access_iterator<vector_of<void, int>::const_iterator>);

export template <class Tag, class Key, class Value>
using dictionary = vector_of<Tag, std::pair<Key, Value>>;

} // namespace js
