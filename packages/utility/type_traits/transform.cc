module;
#include <type_traits>
export module ivm.utility:type_traits.transform;

namespace util {

// Add lvalue reference qualifier if it is not already a reference
export template <class Type>
struct ensure_reference;

export template <class Type>
using ensure_reference_t = ensure_reference<Type>::type;

template <class Type>
struct ensure_reference {
		using type = Type&;
};

template <class Type>
struct ensure_reference<Type&&> {
		using type = Type&&;
};

// T&& -> T
export template <class Type>
struct remove_rvalue_reference;

export template <class Type>
using remove_rvalue_reference_t = remove_rvalue_reference<Type>::type;

template <class Type>
struct remove_rvalue_reference : std::type_identity<Type> {};

template <class Type>
struct remove_rvalue_reference<Type&&> : std::type_identity<Type> {};

// Copy cvref qualifiers of `From` to `To`
template <class From, class To>
struct apply_cvref_impl;

export template <class From, class To>
	requires(!(std::is_const_v<To> || std::is_volatile_v<To> || std::is_reference_v<To>))
struct apply_cvref : apply_cvref_impl<From, To> {};

export template <class From, class To>
using apply_cvref_t = apply_cvref<From, To>::type;

template <class From, class To>
struct apply_cvref_impl : std::type_identity<To> {};

// Generated specializations for `apply_ref_impl`
// NOLINTBEGIN(bugprone-macro-parentheses, cppcoreguidelines-macro-usage)
#define MAKE_CVREF(CV, REF)                  \
	template <class From, class To>            \
	struct apply_cvref_impl<CV From REF, To> { \
			using type = CV To REF;                \
	};                                         \
	// NOLINTEND(bugprone-macro-parentheses, cppcoreguidelines-macro-usage)

MAKE_CVREF(, &)
MAKE_CVREF(, &&)
MAKE_CVREF(const, )
MAKE_CVREF(const, &)
MAKE_CVREF(const, &&)
MAKE_CVREF(volatile, )
MAKE_CVREF(volatile, &)
MAKE_CVREF(volatile, &&)
MAKE_CVREF(const volatile, )
MAKE_CVREF(const volatile, &)
MAKE_CVREF(const volatile, &&)
#undef MAKE_CVREF

} // namespace util
