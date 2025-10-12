module;
#include <utility>
export module isolated_js:dictionary.visit;
import :dictionary.vector_of;
import :recursive_value;
import :transfer;
import ivm.utility;

namespace js {

// Default visitor for non-pair values
template <class Meta, class Type>
struct visit_vector_value : visit_maybe_recursive<Meta, Type> {
		using visit_type = visit_maybe_recursive<Meta, Type>;
		using visit_type::visit_type;
};

// Special case for pairs
template <class Meta, class Key, class Value>
struct visit_vector_value<Meta, std::pair<Key, Value>> {
		constexpr explicit visit_vector_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		visit<Meta, Key> first;
		visit_maybe_recursive<Meta, Value> second;
};

// Entrypoint for `vector_of` visitor. Probably `Entry` is a `std::pair` though maybe it's not
// required.
template <class Meta, class Tag, class Value>
struct visit<Meta, vector_of<Tag, Value>> : visit_vector_value<Meta, Value> {
		using visit_type = visit_vector_value<Meta, Value>;
		using visit_type::visit_type;

		template <class Accept>
		constexpr auto operator()(auto&& value, Accept& accept) const -> accept_target_t<Accept> {
			const visit_type& visitor = *this;
			return accept(Tag{}, visitor, std::forward<decltype(value)>(value));
		}
};

} // namespace js
