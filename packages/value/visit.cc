module;
#include <boost/variant.hpp>
#include <variant>
export module ivm.value:visit;
import :primitive;
import :tag;
import ivm.utility;

namespace ivm::value {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Type>
struct visit {
		constexpr auto operator()(const Type& value, auto accept) -> decltype(auto) {
			return accept(tag_for_t<Type>{}, value);
		}

		constexpr auto operator()(Type&& value, auto accept) -> decltype(auto) {
			return accept(tag_for_t<Type>{}, value);
		}
};

// `visit` delegation which invokes the visitor for the underlying type of a given value.
export template <class Type>
constexpr auto invoke_visit(Type&& value, auto&& accept, auto&&... visit_args) -> decltype(auto) {
	return visit<std::decay_t<Type>>{
		std::forward<decltype(visit_args)>(visit_args)...
	}(value, accept);
}

// Visiting a `boost::variant` visits the underlying member
template <class... Types>
struct visit<boost::variant<Types...>> {
		constexpr auto operator()(auto&& value, auto&& accept) -> decltype(auto) {
			return boost::apply_visitor(
				[ &accept ](auto&& value) constexpr {
					return invoke_visit(
						std::forward<decltype(value)>(value),
						std::forward<decltype(accept)>(accept)
					);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

// `std::variant` visitor. This used to delegate to the `boost::variant` visitor above, but
// `boost::apply_visitor` isn't constexpr, so we can't use it to test statically. `boost::variant`
// can't delegate to this one either because it handles recursive variants.
template <class... Types>
struct visit<std::variant<Types...>> {
		constexpr auto operator()(auto&& value, auto&& accept) -> decltype(auto) {
			return std::visit(
				[ &accept ](auto&& value) constexpr {
					return invoke_visit(
						std::forward<decltype(value)>(value),
						std::forward<decltype(accept)>(accept)
					);
				},
				std::forward<decltype(value)>(value)
			);
		}
};

} // namespace ivm::value
