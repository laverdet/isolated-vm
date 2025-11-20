module;
#include <concepts>
#include <type_traits>
export module util:type_traits.function_traits;
import :type_traits.transform;

namespace util {

// Apply the cvref qualifier of `From` to a function signature
export template <class From, class Fn>
struct apply_function_cvref;

export template <class From, class Fn>
using apply_function_cvref_t = apply_function_cvref<From, Fn>::type;

// Extract the cvref qualifier of a function signature applied to `int`. For example:
// `auto() const& -> void` == `const int&`
export template <class Fn>
struct extract_function_cvref;

export template <class Fn>
using extract_function_cvref_t = extract_function_cvref<Fn>::type;

// Remove cvref qualifier from a function signature. Used as a requirement it approximates
// `std::is_function`, without support for C-style variadic functions.
export template <class Fn>
struct remove_function_cvref;

export template <class Fn>
using remove_function_cvref_t = remove_function_cvref<Fn>::type;

// Generated specializations for `apply_function_cvref`, `extract_function_cvref`, and `remove_function_cvref`
// NOLINTBEGIN(bugprone-macro-parentheses, cppcoreguidelines-macro-usage)
#define MAKE_CVREF(CV, REF)                                                      \
	template <class From, class... Args, bool Nx, class Result>                    \
	struct apply_function_cvref<CV From REF, auto(Args...) noexcept(Nx)->Result> { \
			using type = auto(Args...) CV REF noexcept(Nx) -> Result;                  \
	};                                                                             \
	template <class... Args, bool Nx, class Result>                                \
	struct extract_function_cvref<auto(Args...) CV REF noexcept(Nx)->Result> {     \
			using type = CV int REF;                                                   \
	};                                                                             \
	template <class... Args, bool Nx, class Result>                                \
	struct remove_function_cvref<auto(Args...) CV REF noexcept(Nx)->Result> {      \
			using type = auto(Args...) noexcept(Nx) -> Result;                         \
	};
// NOLINTEND(bugprone-macro-parentheses, cppcoreguidelines-macro-usage)

MAKE_CVREF(, )
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

// Add the given arguments to the front of a function signature
template <class Signature, class... Types>
struct signature_args_push_front;

template <class... Args, bool Nx, class Result, class... Types>
struct signature_args_push_front<auto(Args...) noexcept(Nx)->Result, Types...> {
		using type = auto(Types..., Args...) noexcept(Nx) -> Result;
};

// Given a member function pointer type, extract the required `this` object type
template <class Function>
struct member_invocable_type;

template <class Function>
using member_invocable_type_t = member_invocable_type<Function>::type;

// Traditional member function pointer
template <class Type, class Function>
struct member_invocable_type<Function Type::*>
		: ensure_reference<apply_cvref_t<extract_function_cvref_t<Function>, Type>> {};

// Explicit-this member function
template <class Type, class... Args, bool Nx, class Result>
struct member_invocable_type<auto (*)(Type, Args...) noexcept(Nx)->Result> {
		using type = Type;
};

// Convert a member function pointer type to a free function type, with the object type as the first
// parameter
template <class Type>
struct unbound_member_function_signature;

template <class Type>
using unbound_member_function_signature_t = unbound_member_function_signature<Type>::type;

// Traditional member function pointer
template <class Type, class Function>
struct unbound_member_function_signature<Function Type::*>
		: signature_args_push_front<remove_function_cvref_t<Function>, member_invocable_type_t<Function Type::*>> {};

// Explicit-this member function
template <class... Args, bool Nx, class Result>
struct unbound_member_function_signature<auto (*)(Args...) noexcept(Nx)->Result> {
		using type = auto(Args...) noexcept(Nx) -> Result;
};

// Function signature of any invocable type
export template <class Function>
struct function_signature;

export template <class Function>
using function_signature_t = function_signature<Function>::type;

template <class Type, class Function>
struct function_operator_signature;

// Function type. `std::decay_t` performs function-to-pointer conversion.
template <class Function>
	requires requires { typename remove_function_cvref_t<std::remove_pointer_t<std::decay_t<Function>>>; }
struct function_signature<Function>
		: remove_function_cvref<std::remove_pointer_t<std::decay_t<Function>>> {};

// Struct type with `operator()` (lambda, etc)
template <class Type>
	requires requires { &std::remove_cvref_t<Type>::operator(); }
struct function_signature<Type>
		: function_operator_signature<Type, unbound_member_function_signature_t<decltype(&std::remove_cvref_t<Type>::operator())>> {};

// Member function pointer
template <class Type, class Function>
struct function_signature<Function Type::*>
		: unbound_member_function_signature<Function Type::*> {};

// Strips the first parameter, which is the unbound object type. Also checks invocability of `Type`,
// taking into account cvref qualifiers.
template <class That, class... Args, std::invocable<Args...> Type, bool Nx, class Result>
struct function_operator_signature<Type, auto(That, Args...) noexcept(Nx)->Result> {
		using type = auto(Args...) noexcept(Nx) -> Result;
};

// Canonicalize the cvref type of an invocable. Functions are value types, invocables are whatever
// `this` expects.
export template <class Type>
struct invocable_type;

export template <class Type>
using invocable_type_t = invocable_type<Type>::type;

template <class Function>
	requires requires { typename remove_function_cvref_t<std::remove_pointer_t<std::decay_t<Function>>>; }
struct invocable_type<Function>
		: remove_function_cvref<std::remove_pointer_t<std::decay_t<Function>>> {};

template <class Type>
	requires requires { &std::remove_cvref_t<Type>::operator(); }
struct invocable_type<Type>
		: member_invocable_type<decltype(&std::remove_cvref_t<Type>::operator())> {};

// Result type of a function signature
export template <class Type>
struct signature_result;

export template <class Type>
using signature_result_t = signature_result<Type>::type;

template <class... Args, bool Nx, class Result>
struct signature_result<auto(Args...) noexcept(Nx)->Result> {
		using type = Result;
};

} // namespace util
