module;
#include <utility>
export module isolated_js.visit;
import isolated_js.transfer.types;
import isolated_js.tag;
import ivm.utility;

namespace js {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

// Given `Meta`, this is the type of the root visitor which will be passed as a constructor argument
// to `accept`. It follows that `void` acceptors don't receive this parameter.
export template <class Meta>
using visit_root = visit<Meta, visit_subject_t<Meta>>;

// Automatic creation context which sends the root visitor down to children. Any `visit<M, T>`
// instance should invoke the heritage call operator with itself when passing down to children.
export template <class Meta>
struct visitor_heritage {
		struct child {
				constexpr explicit child(const visit_root<Meta>& visit) :
						visit{visit} {}
				constexpr auto operator()(const auto* /*visit*/) const { return *this; }
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
				const visit_root<Meta>& visit;
		};

		constexpr auto operator()(visit_root<Meta>* visit) const { return child{*visit}; }
};

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		constexpr explicit visit(visitor_heritage<Meta> /*heritage*/, auto&&... args) :
				visit<void, Type>{std::forward<decltype(args)>(args)...} {}
		constexpr explicit visit(visitor_heritage<Meta>::child /*heritage*/, auto&&... args) :
				visit<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `visit` (better error messages)
template <class Type>
struct visit<void, Type>;

// Returns the key type expected by the accept target.
export template <class Meta, util::string_literal Key, class Subject>
struct visit_key_literal {
		visit_key_literal() = default;
		constexpr explicit visit_key_literal(auto /*heritage*/) {}
		constexpr auto get() const { return Key; }
		constexpr auto operator()(const auto& /*nothing*/, const auto& accept) const -> decltype(auto) {
			return accept(string_tag{}, Key);
		}
};

} // namespace js
