module;
#include <type_traits>
export module isolated_js:transfer.types;

namespace js {

// Maps `Subject` or `Target` into the type which will be used to specialize visit / accept context.
// This struct can be specialized for cases in which the root visitor is a subclass of a more
// specific visitor. For example, `v8::FunctionCallbackInfo` -> `v8::Value`.
export template <class Type>
struct accept_target_for : std::type_identity<Type> {};

export template <class Type>
struct visit_subject_for : std::type_identity<Type> {};

// Holder for `Meta` template
export template <class Wrap, class Visit, class Subject, class Accept, class Target>
struct transferee_meta {
		using accept_wrap_type = Wrap;

		using accept_context_type = Accept;
		using accept_target_type = accept_target_for<Target>::type;
		using visit_context_type = Visit;
		using visit_subject_type = visit_subject_for<Subject>::type;
};

} // namespace js
