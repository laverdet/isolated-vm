module;
#include <stdexcept>
#include <type_traits>
#include <utility>
export module ivm.value:transfer;
import :accept;
import :tag;
import :visit;
import ivm.utility;

namespace ivm::value {

// TODO make it private again
// Default `accept` passthrough `Meta`
export struct accept_pass {
		template <class Type>
		using wrap = std::decay_t<Type>;
};

// `Meta` for `accept` which throws on unknown values
struct accept_with_throw {
		template <class Type>
		struct accept_throw {};
		template <class Type>
		using wrap = accept_throw<Type>;
};

// Adds fallback acceptor which throws on unknown values
template <class Meta, class Type>
struct accept<Meta, accept_with_throw::accept_throw<Type>>
		: accept<Meta, std::decay_t<Type>>,
			accept_with_throw::accept_throw<std::decay_t<Type>> {
		using accept<Meta, std::decay_t<Type>>::accept;
		using accept<Meta, std::decay_t<Type>>::operator();
		constexpr auto operator()(value_tag /*tag*/, auto&& /*value*/) const -> Type {
			throw std::logic_error("Type error");
		}
};

// Transfer a JavaScript value from one domain to another
template <class Type, class Meta>
constexpr auto transfer_with(
	auto&& value,
	auto&&... accept_args
) -> decltype(auto) {
	return invoke_visit(
		std::forward<decltype(value)>(value),
		accept<Meta, typename Meta::template wrap<Type>>{
			std::forward<decltype(accept_args)>(accept_args)...
		}
	);
}

// Transfer from unknown types, throws at runtime on unknown type
export template <class Type>
constexpr auto transfer(auto&&... value_and_accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_with_throw>(std::forward<decltype(value_and_accept_args)>(value_and_accept_args)...);
}

// Transfer from known types, won't compile if all paths aren't known
export template <class Type>
constexpr auto transfer_strict(auto&&... value_and_accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(std::forward<decltype(value_and_accept_args)>(value_and_accept_args)...);
}

} // namespace ivm::value
