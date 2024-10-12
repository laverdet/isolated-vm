module;
#include <ranges>
#include <utility>
export module ivm.value:dictionary_visit;
import :dictionary;
import :visit;

namespace ivm::value {

template <class Meta, class Tag, class Key, class Value>
struct accept<Meta, dictionary<Tag, Key, Value>> {
		auto operator()(Tag /*tag*/, auto&& value) const -> dictionary<Tag, Key, Value> {
			auto accept_key = make_accept<Key>(*this);
			auto accept_value = make_accept<Value>(*this);
			return dictionary<Tag, Key, Value>{
				util::into_range(value) |
				std::views::transform([ &accept_key, &accept_value ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::pair{
						invoke_visit(std::forward<decltype(key)>(key), accept_key),
						invoke_visit(std::forward<decltype(value)>(value), accept_value)
					};
				})
			};
		}
};

template <class Tag, class Key, class Value>
struct visit<dictionary<Tag, Key, Value>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value));
		}
};

} // namespace ivm::value
