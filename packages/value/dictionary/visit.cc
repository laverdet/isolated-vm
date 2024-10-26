module;
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
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

// Object key lookup for primitive dictionary variants. This should generally only be used for
// testing, since the subject is basically a C++ heap JSON payload.
template <class Meta, util::string_literal Key, class Type>
struct accept<Meta, value_by_key<Key, Type, void>> {
	public:
		constexpr accept(const auto_visit auto& visit) :
				first{visit},
				second{visit} {}

		constexpr auto operator()(dictionary_tag /*tag*/, const auto& dictionary, const auto& visit) const {
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) {
				return visit.first(entry.first, first) == Key;
			});
			if (it != dictionary.end()) {
				return visit.second(it->second, second);
			} else {
				return second(undefined_in_tag{}, std::monostate{});
			}
		}

	private:
		accept_next<Meta, std::string> first;
		accept_next<Meta, Type> second;
};

} // namespace ivm::value
