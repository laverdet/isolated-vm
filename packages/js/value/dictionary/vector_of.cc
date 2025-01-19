module;
#include <ranges>
#include <vector>
export module isolated_js.dictionary.vector_of;
import ivm.utility;
import isolated_js.tag;

namespace js {

// `vector_of` is just a vector with an associated js tag
export template <class Tag, class Value>
class vector_of : public std::vector<Value> {
	public:
		vector_of() = default;
		consteval vector_of(std::initializer_list<Value> list) :
				std::vector<Value>{list.begin(), list.end()} {}
		consteval explicit vector_of(std::in_place_t /*in_place*/, auto&&... args) {
			this->reserve(sizeof...(args));
			(this->emplace_back(std::forward<decltype(args)>(args)), ...);
		}
		constexpr explicit vector_of(std::ranges::range auto&& range) :
				std::vector<Value>{std::ranges::begin(range), std::ranges::end(range)} {}

		// For testing / assertions
		[[nodiscard]] consteval auto operator==(const vector_of& right) const -> bool {
			const std::vector<Value>& left = *this;
			return left == right;
		}
};

template <class Key, class Value>
vector_of(std::initializer_list<std::pair<Key, Value>>) -> vector_of<dictionary_tag, std::pair<Key, Value>>;

static_assert(std::ranges::range<vector_of<void, int>>);
static_assert(std::random_access_iterator<vector_of<void, int>::const_iterator>);

export template <class Tag, class Key, class Value>
using dictionary = vector_of<Tag, std::pair<Key, Value>>;

} // namespace js
