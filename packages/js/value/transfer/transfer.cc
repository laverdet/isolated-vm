module;
#include <concepts>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.transfer;
export import isolated_js.transfer.types;
export import isolated_js.accept;
export import isolated_js.visit;
import isolated_js.tag;

namespace js {

// Default `accept` passthrough `Wrap`
struct accept_pass {
		template <class Meta, class Type>
		using accept = js::accept<Meta, Type>;
};

// `Wrap` for `accept` which throws on unknown values
struct accept_with_throw {
		template <class Type>
		struct accept_throw;
		template <class Meta, class Type>
		using accept = js::accept<Meta, accept_throw<Type>>;
};

// Adds fallback acceptor which throws on unknown values
template <class Meta, class Type>
// If the requirement fails then a helper struct like `covariant_subject` was instantiated
// probably via `accept_next`
	requires std::destructible<Type>
struct accept<Meta, accept_with_throw::accept_throw<Type>> : accept<Meta, Type> {
		explicit constexpr accept(auto* /*previous*/) :
				accept<Meta, Type>{this} {}

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> Type {
			if constexpr (std::invocable<const accept<Meta, Type>&, decltype(tag), decltype(value), decltype(rest)...>) {
				const accept<Meta, Type>& accept = *this;
				return accept(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
			} else {
				throw std::logic_error{"Type error"};
			}
		}
};

// Instantiates `visit` or `accept` and automatically pass the `this` pointer as the first constructor argument.
template <class VisitOrAccept>
struct transfer_holder : VisitOrAccept {
	private:
		template <size_t... Index>
		constexpr transfer_holder(
			VisitOrAccept* self,
			auto&& args,
			std::index_sequence<Index...> /*index*/
		) :
				VisitOrAccept{self, std::get<Index>(std::forward<decltype(args)>(args))...} {}

	public:
		template <class Args>
		explicit constexpr transfer_holder(std::in_place_t /*in_place*/, Args&& args) :
				transfer_holder{
					this,
					std::forward<decltype(args)>(args),
					std::make_index_sequence<std::tuple_size_v<Args>>{},
				} {}
};

// Select environment type for `accept` or `visit`. This is either `void` for stateless visitors, or
// something like `environment_of<T>` for runtime visitors.
template <class Tuple>
struct select_transferee_environment;

template <class Tuple>
using select_transferee_environment_t = select_transferee_environment<Tuple>::type;

template <>
struct select_transferee_environment<std::tuple<>> : std::type_identity<void> {};

template <class Type, class... Rest>
struct select_transferee_environment<std::tuple<Type, Rest...>> : std::type_identity<std::decay_t<Type>> {};

// Transfer a JavaScript value from one domain to another
template <class Type, class Wrap>
constexpr auto transfer_with(
	auto&& value,
	auto&& visit_args,
	auto&& accept_args
) -> decltype(auto) {
	using subject_type = std::decay_t<decltype(value)>;
	using target_type = std::decay_t<Type>;
	using visit_env = select_transferee_environment_t<std::decay_t<decltype(visit_args)>>;
	using accept_env = select_transferee_environment_t<std::decay_t<decltype(accept_args)>>;
	using meta_holder = transferee_meta<Wrap, visit_env, subject_type, accept_env, target_type>;
	using visit_type = visit<meta_holder, subject_type>;
	using accept_type = Wrap::template accept<meta_holder, Type>;

	transfer_holder<visit_type> visit{std::in_place, std::forward<decltype(visit_args)>(visit_args)};
	transfer_holder<accept_type> accept{std::in_place, std::forward<decltype(accept_args)>(accept_args)};
	return visit(std::forward<decltype(value)>(value), accept);
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

// Transfer from known types, won't compile if all paths aren't known
export template <class Type>
constexpr auto transfer_strict(auto&& value, auto&& visit_args, auto&& accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(
		std::forward<decltype(value)>(value),
		std::forward<decltype(visit_args)>(visit_args),
		std::forward<decltype(accept_args)>(accept_args)
	);
}

// Transfer "out" to a stateless acceptor. No tuple arguments needed.
export template <class Type>
constexpr auto transfer_out(auto&& value, auto&& visit_env, auto&&... visit_args) -> decltype(auto) {
	return transfer<Type>(
		std::forward<decltype(value)>(value),
		std::forward_as_tuple(std::forward<decltype(visit_env)>(visit_env), std::forward<decltype(visit_args)>(visit_args)...),
		std::tuple{}
	);
}

export template <class Type>
constexpr auto transfer_out_strict(auto&& value, auto&& visit_env, auto&&... visit_args) -> decltype(auto) {
	return transfer_strict<Type>(
		std::forward<decltype(value)>(value),
		std::forward_as_tuple(std::forward<decltype(visit_env)>(visit_env), std::forward<decltype(visit_args)>(visit_args)...),
		std::tuple{}
	);
}

// Transfer "in" from a stateless visitor.
export template <class Type>
constexpr auto transfer_in(auto&& value, auto&& accept_env, auto&&... accept_args) -> decltype(auto) {
	return transfer<Type>(
		std::forward<decltype(value)>(value),
		std::tuple{},
		std::forward_as_tuple(std::forward<decltype(accept_env)>(accept_env), std::forward<decltype(accept_args)>(accept_args)...)
	);
}

export template <class Type>
constexpr auto transfer_in_strict(auto&& value, auto&& accept_env, auto&&... accept_args) -> decltype(auto) {
	return transfer_strict<Type>(
		std::forward<decltype(value)>(value),
		std::tuple{},
		std::forward_as_tuple(std::forward<decltype(accept_env)>(accept_env), std::forward<decltype(accept_args)>(accept_args)...)
	);
}

// Only used for testing. Stateless to stateless transfer
export template <class Type>
constexpr auto transfer(auto&& value) -> decltype(auto) {
	return transfer<Type>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
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
		explicit transfer_direct(Type value) :
				value{std::move(value)} {}

		constexpr auto operator*() && -> Type&& { return std::move(value); }
		constexpr auto operator*() const -> const Type& { return value; }

	private:
		Type value;
};

template <class Type>
struct visit<void, transfer_direct<Type>> {
		constexpr auto operator()(auto value, const auto& /*accept*/) const {
			return *std::move(value);
		}
};

} // namespace js
