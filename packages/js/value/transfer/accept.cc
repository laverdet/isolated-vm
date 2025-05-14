module;
#include <type_traits>
#include <utility>
export module isolated_js.accept;
import isolated_js.transfer.types;
import isolated_js.visit;
import ivm.utility;

namespace js {

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// Automatic creation context which sends the previous visitor down to children. Unlike
// `visitor_heritage`, the call operator should only need to be invoked when passing to a recursive
// acceptor.
export template <class Visit>
struct acceptor_heritage {
		using is_heritage = std::true_type;
		template <class Accept>
		struct child {
				using is_heritage = std::true_type;
				constexpr explicit child(const Visit& visit, const Accept& accept) :
						visit{visit},
						accept{accept} {}
				template <class Next>
				constexpr auto operator()(const Next* accept) const { return child<Next>{visit, *accept}; }
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
				const Visit& visit;
				// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
				const Accept& accept;
		};

		constexpr explicit acceptor_heritage(const Visit& visit) :
				visit{visit} {}
		constexpr auto operator()(const auto* accept) const { return child{visit, *accept}; }
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		const Visit& visit;
};

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
		// Swallow `heritage` argument on behalf of non-meta acceptors
		explicit constexpr accept(auto_heritage auto /*heritage*/, auto&&... args) :
				accept<void, Type>{std::forward<decltype(args)>(args)...} {}
};

// Prevent instantiation of non-specialized void-Meta `accept` (better error messages)
template <class Type>
struct accept<void, Type>;

// `accept` with transfer wrapping
export template <class Meta, class Type>
using accept_next = Meta::accept_wrap_type::template accept<Meta, Type>;

// Returns the value corresponding to a key with an accepted object subject.
export template <class Meta, util::string_literal Key, class Type, class Subject>
struct accept_property_value;

} // namespace js
