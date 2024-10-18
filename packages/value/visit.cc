module;
#include <boost/variant.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// Maps `From` or `To` into the type which will be used to specialize visit / accept context.
export template <class Type>
struct transferee_subject : std::type_identity<void> {};

template <class Type>
using transferee_subject_t = transferee_subject<Type>::type;

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
template <class Meta, class Type>
struct wrap_next;

template <class Meta, class Type>
using wrap_next_t = wrap_next<Meta, Type>::type;

template <class Wrap, class From, class To, class Type>
struct wrap_next<std::tuple<Wrap, From, To>, Type>
		: std::type_identity<typename Wrap::template wrap<Type>> {};

export template <class Meta, class Type>
using accept_next = accept<Meta, wrap_next_t<Meta, Type>>;

// Context for dictionary lookup operations
export template <class Meta, auto Key, class Type = Meta>
struct visit_key;

// Unwrap `From` from `Meta`. `Type` defaults to `Meta` which means we want to unwrap the *visit*
// subject.
template <class Wrap, class From, class To, auto Key>
struct visit_key<std::tuple<Wrap, From, To>, Key, std::tuple<Wrap, From, To>>
		: visit_key<std::tuple<Wrap, From, To>, Key, transferee_subject_t<From>> {
		using visit_key<std::tuple<Wrap, From, To>, Key, transferee_subject_t<From>>::visit_key;
};

} // namespace ivm::value
