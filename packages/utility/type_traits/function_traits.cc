module;
#include <type_traits>
#include <utility>
export module ivm.utility:type_traits.function_traits;

namespace util {

// Convert a member function pointer type to a free function type
export template <class Type>
struct unbound_member_function_signature;

export template <class Type>
using unbound_member_function_signature_t = unbound_member_function_signature<Type>::type;

template <class Type, class Function>
struct unbound_member_function_signature_impl;

template <class Type>
struct unbound_member_function_signature {
		static_assert(false, "`Type` is not a member function");
};

template <class Type, class Function>
struct unbound_member_function_signature<Function Type::*> : unbound_member_function_signature_impl<Type, Function> {};

// Generated cv-ref specialized implementations
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define UNBIND_MEMBER_FUNCTION(CV, REF_AS, REF)                                                    \
	template <class Type, class... Args, bool Nx, class Result>                                      \
	struct unbound_member_function_signature_impl<Type, auto(Args...) CV REF noexcept(Nx)->Result> { \
			using type = auto(CV Type REF_AS, Args...) noexcept(Nx) -> Result;                           \
	};

UNBIND_MEMBER_FUNCTION(, &, )
UNBIND_MEMBER_FUNCTION(, &, &)
UNBIND_MEMBER_FUNCTION(, &&, &&)
UNBIND_MEMBER_FUNCTION(const, &, )
UNBIND_MEMBER_FUNCTION(const, &, &)
UNBIND_MEMBER_FUNCTION(const, &&, &&)
UNBIND_MEMBER_FUNCTION(volatile, &, )
UNBIND_MEMBER_FUNCTION(volatile, &, &)
UNBIND_MEMBER_FUNCTION(volatile, &&, &&)
UNBIND_MEMBER_FUNCTION(const volatile, &, )
UNBIND_MEMBER_FUNCTION(const volatile, &, &)
UNBIND_MEMBER_FUNCTION(const volatile, &&, &&)

// Function signature of any invocable type
export template <class Function>
struct function_signature;

export template <class Function>
using function_signature_t = function_signature<Function>::type;

template <class Function>
struct function_signature_function_impl;

template <class Type, class Function>
struct function_signature_operator_impl;

// Struct type with `operator()` (lambda, etc)
template <class Function>
	requires std::is_class_v<Function>
struct function_signature<Function> : function_signature_operator_impl<Function, unbound_member_function_signature_t<decltype(&Function::operator())>> {};

// Function type
template <class... Args, bool Nx, class Result>
struct function_signature<auto(Args...) noexcept(Nx)->Result> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

// Reference to function
template <class Function>
struct function_signature<Function&> : function_signature_function_impl<Function> {};

// Pointer to function
template <class Function>
struct function_signature<Function*> : function_signature_function_impl<Function> {};

// Member function pointer
template <class Type, class Function>
struct function_signature<Function Type::*> : unbound_member_function_signature_impl<Type, Function> {};

// This is the same as the `function_signature` specialization. It's split out to avoid the
// pointer-to-function specialization from stripping off a ref qualifier on an invocable object type
// which would affect invocability.
template <class... Args, bool Nx, class Result>
struct function_signature_function_impl<auto(Args...) noexcept(Nx)->Result> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

// Strips the first parameter, which is the unbound object type. Also checks cv-ref qualifiers with
// `requires`.
template <class Type, class That, class... Args, bool Nx, class Result>
	requires requires { std::declval<Type>()(std::declval<Args>()...); }
struct function_signature_operator_impl<Type, auto(That, Args...) noexcept(Nx)->Result> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

// Result type of a function signature
export template <class Type>
struct signature_result;

export template <class Type>
using signature_result_t = signature_result<Type>::type;

template <class... Args, bool Nx, class Result>
struct signature_result<auto(Args...) noexcept(Nx)->Result> : std::type_identity<Result> {};

} // namespace util
