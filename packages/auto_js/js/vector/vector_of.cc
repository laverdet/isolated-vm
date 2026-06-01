export module auto_js:vector.vector_of;
import :tag;
import std;

namespace js {

// Declares that this type is a transferrable range
export template <class Type>
struct tagged_range;

// Or, define `range_tag_type` on the type itself
template <class Type>
	requires(Type::range_tag_type)
struct tagged_range<Type> : std::type_identity<typename Type::range_tag_type> {};

// Check whether or not `tagged_range` is valid for this type
template <class Type>
concept transferable_range = std::convertible_to<typename tagged_range<Type>::type, object_tag>;

// std::vector<T> is a transferrable range
template <class Type, class Alloc>
struct tagged_range<std::vector<Type, Alloc>> : std::type_identity<vector_tag> {};

// `vector_of` is just a vector with an associated js tag
export template <class Tag, class Value>
class vector_of : public std::vector<Value> {
	public:
		vector_of() = default;

		consteval explicit vector_of(std::in_place_t /*in_place*/, auto&&... args) {
			this->reserve(sizeof...(args));
			(..., this->emplace_back(std::forward<decltype(args)>(args)));
		}

		constexpr explicit vector_of(std::from_range_t from_range, auto&& range) :
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
