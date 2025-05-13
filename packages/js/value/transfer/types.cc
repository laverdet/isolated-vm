module;
#include <type_traits>
export module isolated_js.transfer.types;

namespace js {

// Maps `Subject` or `Target` into the type which will be used to specialize visit / accept context.
// This struct can be specialized for cases in which the root visitor is a subclass of a more
// specific visitor. For example, `v8::FunctionCallbackInfo` -> `v8::Value`.
export template <class Type>
struct transferee_subject : std::type_identity<Type> {};

template <class Type>
using transferee_subject_t = transferee_subject<Type>::type;

// Holder for `Meta` template
export template <class Wrap, class Subject, class Target>
struct transferee_meta {
		using accept_target_type = transferee_subject_t<Target>;
		using accept_wrap_type = Wrap;
		using visit_subject_type = transferee_subject_t<Subject>;
};

// Requirement for any "heritage" type used in `visit` & `accept`.
export template <class Type>
concept auto_heritage = requires {
	typename Type::is_heritage;
};

} // namespace js
