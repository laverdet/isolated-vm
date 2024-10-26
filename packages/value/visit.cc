module;
#include <boost/variant.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// Holder for `Meta` template
export template <class Wrap, class Subject, class Target>
struct transferee_meta;

// Maps `Subject` or `Target` into the type which will be used to specialize visit / accept context.
export template <class Type>
struct transferee_subject : std::type_identity<void> {};

template <class Type>
using transferee_subject_t = transferee_subject<Type>::type;

// Extract `Wrap` from `Meta`
template <class Meta>
struct select_wrap;

template <class Meta>
using select_wrap_t = select_wrap<Meta>::type;

template <class Wrap, class Subject, class Target>
struct select_wrap<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<Wrap> {};

// Extract `Target` from `Meta`, which is the `Type` passed to the root `accept` instance
template <class Meta>
struct accept_target;

export template <class Meta>
using accept_target_t = accept_target<Meta>::type;

template <class Wrap, class Subject, class Target>
struct accept_target<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<transferee_subject_t<Target>> {};

// Extract `Subject` from `Meta`, which is the `Type` passed to the root `visit` instance
template <class Meta>
struct visit_subject;

export template <class Meta>
using visit_subject_t = visit_subject<Meta>::type;

template <class Wrap, class Subject, class Target>
struct visit_subject<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<transferee_subject_t<Subject>> {};

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

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// Concept for any `accept`
template <class Type>
struct is_accept : std::bool_constant<true> {};

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
export template <class Meta, class Type>
using accept_next = select_wrap_t<Meta>::template accept<Meta, Type>;

// Used by a visitor to returns the immediate accepted value matching a tag.
export template <class Tag>
struct accept_immediate;

template <class Tag>
struct accept<void, accept_immediate<Tag>> {
		constexpr auto operator()(Tag /*tag*/, auto&& value) const -> decltype(auto) {
			return std::forward<decltype(value)>(value);
		}
};

// Returns the key type expected by the accept target.
export template <util::string_literal Key, class Subject>
struct key_for;

// Returns the value corresponding to a key with an accepted object subject.
export template <util::string_literal Key, class Type, class Subject>
struct value_by_key;

} // namespace ivm::value
