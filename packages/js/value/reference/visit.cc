export module isolated_js:reference.visit;
import :recursive_value;
import :transfer;

namespace js {

// `recursive_value` visitor delegates to its underlying value visitor.
template <class Meta, template <class> class Make>
struct visit<Meta, recursive_value<Make>> : visit<Meta, typename recursive_value<Make>::value_type> {
		using visit_type = visit<Meta, typename recursive_value<Make>::value_type>;
		using visit_type::visit_type;
};

} // namespace js
