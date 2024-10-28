module;
#include <boost/variant.hpp>
#include <type_traits>
export module ivm.value:dictionary.helpers;

namespace ivm::value {

// Look for `boost::recursive_variant_` to determine if this container is recursive
export template <class Type>
struct is_recursive : std::bool_constant<false> {};

template <class Type>
constexpr auto is_recursive_v = is_recursive<Type>::value;

template <>
struct is_recursive<boost::recursive_variant_> : std::bool_constant<true> {};

template <template <class...> class Type, class... Types>
struct is_recursive<Type<Types...>>
		: std::bool_constant<std::disjunction_v<is_recursive<Types>...>> {};

// Helper to instantiate a different `accept` or `visit` structure for recursive containers
export template <class Type>
struct entry_subject;

template <class Entry>
struct entry_subject_for;

export template <class Entry>
using entry_subject_for_t = entry_subject_for<Entry>::type;

template <class Entry>
struct entry_subject_for
		: std::type_identity<entry_subject<Entry>> {};

// Handles the special case of `std::pair` elements. Since the "key"/`first` member won't be
// recursive, but the "value"/`second` member *may* be recursive. This helps to break the pair apart
// and applies the `entry_subject` helper to both members.
template <class Left, class Right>
struct entry_subject_for<std::pair<Left, Right>>
		: std::type_identity<std::pair<entry_subject<Left>, entry_subject<Right>>> {};

// Specialization helper for `vector_of`
export template <class Tag, class Type, class Entry>
struct vector_of_subject;

} // namespace ivm::value
