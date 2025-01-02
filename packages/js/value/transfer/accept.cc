module;
#include <type_traits>
#include <utility>
export module ivm.value:accept;
import :transfer.types;
import :visit;

namespace ivm::value {

// Extract `Wrap` from `Meta`
template <class Meta>
struct select_wrap;

template <class Meta>
using select_wrap_t = select_wrap<Meta>::type;

template <class Wrap, class Subject, class Target>
struct select_wrap<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<Wrap> {};

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
		explicit constexpr accept(const visit_root<Meta>& /*visit*/, auto&&... args) :
				accept<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Base specialization for stateless acceptors. This just gets you the `auto_accept` constructor
// used by recursive acceptors.
template <>
struct accept<void, void> {
		accept() = default;
		// Recursive acceptor constructor
		constexpr accept(int /*dummy*/, const auto& /*visit*/, const auto_accept auto& /*accept*/) {}
};

// `accept` with transfer wrapping
export template <class Meta, class Type>
using accept_next = select_wrap_t<Meta>::template accept<Meta, Type>;

// Used by a visitor to return the immediate accepted value matching a tag.
export template <class Tag>
struct accept_immediate;

template <class Tag>
struct accept<void, accept_immediate<Tag>> {
		constexpr auto operator()(Tag /*tag*/, auto&& value) const -> decltype(auto) {
			return std::forward<decltype(value)>(value);
		}
};

} // namespace ivm::value
