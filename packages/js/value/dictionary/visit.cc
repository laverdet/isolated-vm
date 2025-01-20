module;
#include <algorithm>
#include <string_view>
#include <utility>
#include <variant>
export module isolated_js.dictionary.visit;
import isolated_js.dictionary.helpers;
import isolated_js.dictionary.vector_of;
import isolated_js.tag;
import isolated_js.transfer;
import ivm.utility;

namespace js {

// Non-recursive member visitor
template <class Meta, class Type>
struct visit_entry_value : visit<Meta, Type> {
		using visit<Meta, Type>::visit;
};

// Recursive member
template <class Meta, class Type>
	requires is_recursive_v<Type>
struct visit_entry_value<Meta, Type> {
	public:
		constexpr explicit visit_entry_value(auto_heritage auto visit_heritage) :
				visit_{&visit_heritage.visit} {}

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return (*visit_)(std::forward<decltype(value)>(value), accept);
		}

	private:
		const visit_root<Meta>* visit_;
};

// Default visitor for non-pair values
template <class Meta, class Type>
struct visit_vector_value : visit_entry_value<Meta, Type> {
		using visit_entry_value<Meta, Type>::visit_entry_value;
};

// Special case for pairs
template <class Meta, class Key, class Value>
struct visit_vector_value<Meta, std::pair<Key, Value>> {
		constexpr explicit visit_vector_value(auto_heritage auto visit_heritage) :
				first{visit_heritage},
				second{visit_heritage} {}

		visit<Meta, Key> first;
		visit_entry_value<Meta, Value> second;
};

// Entrypoint for `vector_of` visitor. Probably `Entry` is a `std::pair` though maybe it's not
// required.
template <class Meta, class Tag, class Value>
struct visit<Meta, vector_of<Tag, Value>> : visit_vector_value<Meta, Value> {
		constexpr explicit visit(auto_heritage auto visit_heritage) :
				visit_vector_value<Meta, Value>{visit_heritage(this)} {}

		constexpr auto operator()(auto&& value, const auto_accept auto& accept) const -> decltype(auto) {
			const visit_vector_value<Meta, Value>& visitor = *this;
			return accept(Tag{}, std::forward<decltype(value)>(value), visitor);
		}
};

// Object key lookup for primitive dictionary variants. This should generally only be used for
// testing, since the subject is basically a C++ heap JSON payload.
template <class Meta, util::string_literal Key, class Type, class Tag, class KeyType, class Value>
struct accept_property_value<Meta, Key, Type, vector_of<Tag, std::pair<KeyType, Value>>> {
	public:
		explicit constexpr accept_property_value(auto_heritage auto accept_heritage) :
				first{accept_heritage},
				second{accept_heritage} {}

		constexpr auto operator()(dictionary_tag /*tag*/, const auto& dictionary, const auto& visit) const {
			auto it = std::ranges::find_if(dictionary, [ & ](const auto& entry) {
				return visit.first(entry.first, first) == Key;
			});
			if (it == dictionary.end()) {
				return second(undefined_in_tag{}, std::monostate{});
			} else {
				return visit.second(it->second, second);
			}
		}

	private:
		accept_next<Meta, std::string_view> first;
		accept_next<Meta, Type> second;
};

} // namespace js
