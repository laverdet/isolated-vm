module;
#include <boost/variant.hpp>
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.value:visit;
import :tag;
import ivm.utility;

namespace ivm::value {

// `visit` accepts a value and acceptor and then invokes the acceptor with a JavaScript type tag and
// a value which follows some sort of casting interface corresponding to the tag.
export template <class Type>
struct visit;

// TODO: remove
export template <class Type>
constexpr auto invoke_visit(Type&& value, const auto& accept) -> decltype(auto) {
	return visit<std::decay_t<Type>>{}(std::forward<decltype(value)>(value), accept);
}

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept : accept<void, Type> {
		using accept<void, Type>::accept;
};

// `accept` with transfer wrapping
export template <class Meta, class Type>
using accept_next = accept<Meta, typename Meta::template wrap<Type>>;

} // namespace ivm::value
