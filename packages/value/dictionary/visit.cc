module;
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
export module ivm.value:dictionary.visit;
import :dictionary.types;
import :dictionary.vector_of;
import :visit;
import ivm.utility;

namespace ivm::value {

// Non-recursive visitor
template <class Meta, class Type>
struct visit<Meta, dictionary_subject<Type>> : visit<Meta, Type> {
		visit() = default;
		constexpr visit(int /*dummy*/, const auto_visit auto& /*visit*/) {}
};

// Recursive visitor
template <class Meta, class Type>
	requires is_recursive_v<Type>
struct visit<Meta, dictionary_subject<Type>> {
	public:
		visit() = delete;
		constexpr visit(int /*dummy*/, const auto_visit auto& visit) :
				visit_{&visit} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return (*visit_)(std::forward<decltype(value)>(value), accept);
		}

	private:
		const visit<Meta, Type>* visit_;
};

// Entrypoint for `dictionary`
template <class Meta, class Tag, class Key, class Value>
struct visit<Meta, dictionary<Tag, Key, Value>> {
		visit() = default;
		constexpr visit(int dummy, const auto_visit auto& visit) :
				first{dummy, visit},
				second{dummy, visit} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(Tag{}, std::forward<decltype(value)>(value), *this);
		}

		visit<Meta, dictionary_subject<Key>> first;
		visit<Meta, dictionary_subject<Value>> second;
};

} // namespace ivm::value
