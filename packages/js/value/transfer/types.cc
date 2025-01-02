module;
#include <type_traits>
export module ivm.value:transfer.types;
import ivm.utility;

namespace ivm::value {

// Holder for `Meta` template
export template <class Wrap, class Subject, class Target>
struct transferee_meta;

// Maps `Subject` or `Target` into the type which will be used to specialize visit / accept context.
// This struct can be specialized for cases in which the root visitor is a subclass of a more
// specific visitor. For example, `v8::FunctionCallbackInfo` -> `v8::Value`.
export template <class Type>
struct transferee_subject : std::type_identity<Type> {};

template <class Type>
using transferee_subject_t = transferee_subject<Type>::type;

// Returns the key type expected by the accept target.
export template <util::string_literal Key, class Subject>
struct key_for;

// Returns the value corresponding to a key with an accepted object subject.
export template <util::string_literal Key, class Type, class Subject>
struct value_by_key;

// Extract `Target` from `Meta` (which is the `Type` passed to the root `accept` instance) and map
// to transferee subject
template <class Meta>
struct accept_target;

export template <class Meta>
using accept_target_t = accept_target<Meta>::type;

template <class Wrap, class Subject, class Target>
struct accept_target<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<transferee_subject_t<Target>> {};

// Extract `Subject` from `Meta` (which is the `Type` passed to the root `visit` instance) and map
// to transferee subject
template <class Meta>
struct visit_subject;

export template <class Meta>
using visit_subject_t = visit_subject<Meta>::type;

template <class Wrap, class Subject, class Target>
struct visit_subject<transferee_meta<Wrap, Subject, Target>>
		: std::type_identity<transferee_subject_t<Subject>> {};

} // namespace ivm::value
