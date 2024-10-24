module;
#include <boost/variant.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// Maps `Subject` or `Target` into the type which will be used to specialize visit / accept context.
export template <class Type>
struct transferee_subject : std::type_identity<void> {};

template <class Type>
using transferee_subject_t = transferee_subject<Type>::type;

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

// Used by acceptors w/ context to store reference to the root visitor
template <class Meta>
struct visit_subject;

template <class Wrap, class Subject, class Target>
struct visit_subject<std::tuple<Wrap, Subject, Target>>
		: std::type_identity<visit<std::tuple<Wrap, Subject, Target>, Subject>> {};

export template <class Meta>
using visit_subject_t = visit_subject<Meta>::type;

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// Concept for any `accept`
template <class Type>
struct is_accept : std::bool_constant<false> {};

template <class Meta, class Type>
struct is_accept<accept<Meta, Type>> : std::bool_constant<true> {};

template <class Type>
constexpr bool is_accept_v = is_accept<Type>::value;

export template <class Type>
concept auto_accept = is_accept_v<Type>;

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
		// Swallow `visit` argument on behalf of non-meta acceptors
		constexpr accept(const auto_visit auto& /*visit*/, auto&&... args) :
				accept<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Base specialization for stateless acceptors. This just gets you the `dummy` constructor used by
// recursive acceptors.
template <>
struct accept<void, void> {
		accept() = default;
		// Recursive acceptor constructor
		constexpr accept(int /*dummy*/, const auto_visit auto& /*visit*/, const auto_accept auto& /*accept*/) {}
};

// `accept` with transfer wrapping
template <class Meta, class Type>
struct wrap_next;

template <class Meta, class Type>
using wrap_next_t = wrap_next<Meta, Type>::type;

template <class Wrap, class Subject, class Target, class Type>
struct wrap_next<std::tuple<Wrap, Subject, Target>, Type>
		: std::type_identity<typename Wrap::template wrap<Type>> {};

export template <class Meta, class Type>
using accept_next = accept<Meta, wrap_next_t<Meta, Type>>;

// Context for dictionary lookup operations
export template <class Meta, util::string_literal Key, class Type = Meta>
struct visit_key;

// Default `visit_key` swallows `Meta`
template <class Meta, util::string_literal Key, class Type>
struct visit_key : visit_key<void, Key, Type> {
		// Swallow `visit` argument on behalf of non-meta visitors
		constexpr visit_key(const auto_visit auto& /*visit*/) {}
};

// Unwrap `Subject` from `Meta`. `Type` defaults to `Meta` which means we want to unwrap the *visit*
// subject.
template <class Wrap, class Subject, class Target, auto Key>
struct visit_key<std::tuple<Wrap, Subject, Target>, Key, std::tuple<Wrap, Subject, Target>>
		: visit_key<std::tuple<Wrap, Subject, Target>, Key, transferee_subject_t<Subject>> {
		using visit_key<std::tuple<Wrap, Subject, Target>, Key, transferee_subject_t<Subject>>::visit_key;
};

} // namespace ivm::value
