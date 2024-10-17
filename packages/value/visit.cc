module;
#include <boost/variant.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Type>
struct visit;

// Base specialization for stateless visitors.
template <>
struct visit<void> {
		visit() = default;
		// Recursive visitor constructor
		constexpr visit(int /*dummy*/, const auto& /*visit*/) {}
};

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
};

// Base specialization for stateless acceptors. This just gets you the `dummy` constructor used by
// recursive acceptors.
template <>
struct accept<void, void> {
		accept() = default;
		// Recursive acceptor constructor
		constexpr accept(int /*dummy*/, const auto& /*accept*/) {}
};

// `accept` with transfer wrapping
export template <class Meta, class Type>
using accept_next = accept<Meta, typename Meta::template wrap<Type>>;

} // namespace ivm::value
