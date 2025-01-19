module;
#include <type_traits>
#include <utility>
export module isolated_js.accept;
import isolated_js.transfer.types;
import isolated_js.visit;

namespace js {

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
template <class Meta, class Type>
struct is_accept<accept<Meta, Type>> : std::true_type {};

export template <class Type>
concept auto_accept = is_accept_v<Type>;

// Automatic creation context which sends the previous visitor down to children. Unlike
// `visitor_heritage`, the call operator should only need to be invoked when passing to a recursive
// acceptor.
export template <class Meta>
struct acceptor_heritage {
		template <class Accept>
		struct child {
				constexpr explicit child(const visit_root<Meta>& visit, const Accept& accept) :
						visit{visit},
						accept{accept} {}
				template <class Next>
				constexpr auto operator()(const Next* accept) const { return child<Next>{visit, *accept}; }
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
				const visit_root<Meta>& visit;
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
				const Accept& accept;
		};

		constexpr explicit acceptor_heritage(const visit_root<Meta>& visit) :
				visit{visit} {}
		constexpr auto operator()(const auto* accept) const { return child{visit, *accept}; }
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		const visit_root<Meta>& visit;
};

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
		// Swallow `heritage` argument on behalf of non-meta acceptors
		explicit constexpr accept(auto /*heritage*/, auto&&... args) :
				accept<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `accept` (better error messages)
template <class Type>
struct accept<void, Type>;

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

} // namespace js
