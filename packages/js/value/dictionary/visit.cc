module;
#include <utility>
export module isolated_js:dictionary.visit;
import :dictionary.helpers;
import :dictionary.vector_of;
import :transfer;
import ivm.utility;

namespace js {

// Default visitor for non-pair values
template <class Meta, class Type>
struct visit_vector_value : visit<Meta, Type> {
		using visit<Meta, Type>::visit;
};

// Recursive member
template <class Meta, class Type>
	requires is_recursive_v<Type>
struct visit_vector_value<Meta, Type> {
	public:
		constexpr explicit visit_vector_value(auto* transfer) :
				visit_{*transfer} {}

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			return visit_(std::forward<decltype(value)>(value), accept);
		}

	private:
		std::reference_wrapper<const visit<Meta, typename Meta::visit_subject_type>> visit_;
};

// Special case for pairs
template <class Meta, class Key, class Value>
struct visit_vector_value<Meta, std::pair<Key, Value>> {
		constexpr explicit visit_vector_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		visit<Meta, Key> first;
		visit_vector_value<Meta, Value> second;
};

// Entrypoint for `vector_of` visitor. Probably `Entry` is a `std::pair` though maybe it's not
// required.
template <class Meta, class Tag, class Value>
struct visit<Meta, vector_of<Tag, Value>> : visit_vector_value<Meta, Value> {
		using visit_type = visit_vector_value<Meta, Value>;
		using visit_type::visit_type;

		constexpr auto operator()(auto&& value, auto& accept) const -> decltype(auto) {
			const visit_type& visitor = *this;
			return invoke_accept(accept, Tag{}, visitor, std::forward<decltype(value)>(value));
		}
};

} // namespace js
