module;
#include <ranges>
#include <vector>
export module ivm.value:dictionary;
import :accept;
import :visit;
import ivm.utility;

namespace ivm::value {

export template <class Tag, class Key, class Value>
class dictionary_value {
	public:
		using value_type = std::pair<Key, Value>;
		using container_type = std::vector<value_type>;
		using const_iterator = typename container_type::const_iterator;
		using iterator = typename container_type::iterator;

		dictionary_value() = default;
		explicit dictionary_value(std::ranges::range auto&& range) :
				values{std::ranges::begin(range), std::ranges::end(range)} {}

		[[nodiscard]] auto begin() -> iterator { return values.begin(); }
		[[nodiscard]] auto begin() const -> const_iterator { return values.begin(); }
		[[nodiscard]] auto end() -> iterator { return values.end(); }
		[[nodiscard]] auto end() const -> const_iterator { return values.end(); }

	private:
		container_type values;
};

static_assert(std::ranges::range<dictionary_value<void, int, int>>);
static_assert(std::random_access_iterator<dictionary_value<void, int, int>::const_iterator>);

template <class Meta, class Tag, class Key, class Value>
struct accept<Meta, dictionary_value<Tag, Key, Value>> {
		auto operator()(Tag /*tag*/, auto&& dictionary) const -> dictionary_value<Tag, Key, Value> {
			auto accept_key = make_accept<Key>(*this);
			auto accept_value = make_accept<Value>(*this);
			return dictionary_value<Tag, Key, Value>{
				into_range(dictionary) |
				std::views::transform([ &accept_key, &accept_value ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::make_pair(
						invoke_visit(std::forward<decltype(key)>(key), accept_key),
						invoke_visit(std::forward<decltype(value)>(value), accept_value)
					);
				})
			};
		}
};

template <class Tag, class Key, class Value>
struct visit<dictionary_value<Tag, Key, Value>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value));
		}
};

} // namespace ivm::value
