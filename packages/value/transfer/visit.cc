module;
#include <type_traits>
export module ivm.value:visit;
import :transfer.types;

namespace ivm::value {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

// Concept for any `visit`
template <class Type>
struct is_visit : std::bool_constant<false> {};

template <class Meta, class Type>
struct is_visit<visit<Meta, Type>> : std::bool_constant<true> {};

template <class Type>
constexpr bool is_visit_v = is_visit<Type>::value;

export template <class Type>
concept auto_visit = is_visit_v<Type>;

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		using visit<void, Type>::visit;
};

// Base specialization for stateless visitors.
template <>
struct visit<void, void> {
		visit() = default;
		// Recursive visitor constructor
		constexpr visit(int /*dummy*/, const auto& /*visit*/) {}
};

} // namespace ivm::value
