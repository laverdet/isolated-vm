export module ivm.js:visit;
import :transfer.types;

namespace ivm::js {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Meta, class Type>
struct visit;

// Given `Meta`, this is the type of the root visitor which will be passed as a constructor argument
// to `accept`. It follows that `void` acceptors don't receive this parameter.
export template <class Meta>
using visit_root = visit<Meta, visit_subject_t<Meta>>;

// Default `visit` swallows `Meta`
template <class Meta, class Type>
struct visit : visit<void, Type> {
		using visit<void, Type>::visit;
};

// Base specialization for stateless visitors.
template <>
struct visit<void, void> {
		visit() = default;
		// Recursive visitor constructor
		constexpr visit(int /*dummy*/, const auto& /*visit*/) {}
};

} // namespace ivm::js
