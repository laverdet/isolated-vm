module;
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.value:transfer;
export import :transfer.types;
export import :accept;
export import :visit;
import :tag;

namespace ivm::value {

// Default `accept` passthrough `Meta`
struct accept_pass {
		template <class Meta, class Type>
		using accept = value::accept<Meta, std::decay_t<Type>>;
};

// `Meta` for `accept` which throws on unknown values
struct accept_with_throw {
		template <class Type>
		struct accept_throw;
		template <class Meta, class Type>
		using accept = value::accept<Meta, accept_throw<Type>>;
};

// Adds fallback acceptor which throws on unknown values
template <class Meta, class Type>
// If the requirement fails then a helper struct like `covariant_subject` was instantiated
// probably via `accept_next`
	requires std::destructible<Type>
struct accept<Meta, accept_with_throw::accept_throw<Type>>
		: accept<Meta, std::decay_t<Type>> {
		explicit constexpr accept(const auto_visit auto& visit) :
				accept<Meta, std::decay_t<Type>>{0, visit, *this} {}

		using accept<Meta, std::decay_t<Type>>::operator();
		constexpr auto operator()(value_tag /*tag*/, auto&& /*value*/, auto&&... /*rest*/) const -> Type {
			throw std::logic_error("Type error");
		}
};

// Transfer a JavaScript value from one domain to another
template <class Type, class Wrap>
constexpr auto transfer_with(
	auto&& value,
	auto&& visit_args,
	auto&& accept_args
) -> decltype(auto) {
	using from_type = std::decay_t<decltype(value)>;
	using meta_holder = transferee_meta<Wrap, from_type, std::decay_t<Type>>;
	using visit_type = visit<meta_holder, from_type>;
	using accept_type = Wrap::template accept<meta_holder, Type>;
	auto visitor = std::make_from_tuple<visit_type>(std::forward<decltype(visit_args)>(visit_args));
	auto acceptor = std::make_from_tuple<accept_type>(std::tuple_cat(
		std::forward_as_tuple(visitor),
		std::forward<decltype(accept_args)>(accept_args)
	));
	return visitor(std::forward<decltype(value)>(value), acceptor);
}

// Transfer from unknown types, throws at runtime on unknown type
export template <class Type>
constexpr auto transfer(auto&& value, auto&& visit_args, auto&& accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_with_throw>(
		std::forward<decltype(value)>(value),
		std::forward<decltype(visit_args)>(visit_args),
		std::forward<decltype(accept_args)>(accept_args)
	);
}

export template <class Type>
constexpr auto transfer(auto&& value) -> decltype(auto) {
	return transfer<Type>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
}

// Transfer from known types, won't compile if all paths aren't known
export template <class Type>
constexpr auto transfer_strict(auto&& value, auto&& visit_args, auto&& accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(
		std::forward<decltype(value)>(value),
		std::forward<decltype(visit_args)>(visit_args),
		std::forward<decltype(accept_args)>(accept_args)
	);
}

export template <class Type>
constexpr auto transfer_strict(auto&& value) -> decltype(auto) {
	return transfer_strict<Type>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
}

// Allows the invoker of `transfer` to pass through a value directly without invoking `visit` or
// `accept`. For example, as a directly created element of an array.
export template <class Type>
struct transfer_direct {
	public:
		explicit transfer_direct(Type&& value) :
				value{std::move(value)} {}

		constexpr auto operator*() && -> Type&& {
			return std::move(value);
		}

	private:
		Type value;
};

template <class Type>
struct visit<void, transfer_direct<Type>> {
		constexpr auto operator()(auto value, const auto_accept auto& /*accept*/) const {
			return *std::move(value);
		}
};

} // namespace ivm::value
