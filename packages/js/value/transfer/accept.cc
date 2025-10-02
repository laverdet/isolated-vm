module;
#include <concepts>
#include <utility>
export module isolated_js:accept;
import :transfer.types;
import :visit;
import ivm.utility;

namespace js {

// Default `accept` swallows `Meta`
template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
		// Swallow `previous` argument on behalf of non-meta acceptors
		explicit constexpr accept(auto* /*previous*/, auto&&... args) :
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
