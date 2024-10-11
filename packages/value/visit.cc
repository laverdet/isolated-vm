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

// Base stateless visitor
template <>
struct visit<void> {
		visit() = default;
		// Copy constructors, or any constructors which begin with a reference to themselves, cannot be
		// inherited. The extra `int` allows this "copy constructor" to be inherited.
		constexpr visit(int /*ignore*/, const visit& that) :
				visit{that} {}
};

// Construct a new stateless or trivial visitor from an existing visitor
export template <class Type>
constexpr auto beget_visit(auto&& parent_visit) {
	return visit<std::decay_t<Type>>{0, std::forward<decltype(parent_visit)>(parent_visit)};
}

// `visit` delegation which invokes the visitor for the underlying type of a given value. Generally
// this should only be used for stateless or trivial visitors.
export template <class Type>
constexpr auto invoke_visit(const auto& parent_visitor, Type&& value, const auto& accept) -> decltype(auto) {
	return beget_visit<Type>(parent_visitor)(std::forward<decltype(value)>(value), accept);
}

// TODO: remove
export template <class Type>
constexpr auto invoke_visit(Type&& value, const auto& accept) -> decltype(auto) {
	return beget_visit<Type>(visit<void>{})(std::forward<decltype(value)>(value), accept);
}

// `accept` is the target of `visit`
export template <class Meta, class Type>
struct accept;

// Convert an existing acceptor into a new one which accepts a different type
export template <class Next, class Meta, class Type>
constexpr auto make_accept(const accept<Meta, Type>& accept_from) {
	using accept_type = accept<Meta, typename Meta::template wrap<Next>>;
	if constexpr (std::constructible_from<accept_type, const accept<Meta, Type>&>) {
		return accept_type{accept_from};
	} else {
		static_assert(std::is_default_constructible_v<accept_type>);
		return accept_type{};
	}
}

// Delegate to another acceptor by a different type. Does not wrap in the same way `make_accept`
// does.
export template <class Next, class Meta, class Type>
constexpr auto delegate_accept(
	const accept<Meta, Type>& /*accept_from*/,
	auto tag,
	auto&& value,
	auto&&... accept_args
) -> decltype(auto) {
	using accept_type = accept<Meta, Next>;
	return accept_type{
		std::forward<decltype(accept_args)>(accept_args)...
	}(
		tag,
		std::forward<decltype(value)>(value)
	);
}

} // namespace ivm::value
