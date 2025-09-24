module;
#include <concepts>
#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js.transfer;
export import isolated_js.transfer.types;
export import isolated_js.accept;
export import isolated_js.visit;
import isolated_js.tag;
import ivm.utility;

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
		using accept_type = accept<Meta, Type>;
		explicit constexpr accept(auto* /*previous*/) :
				accept_type{this} {}

		constexpr auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> Type
			requires std::invocable<const accept_type&, decltype(tag), decltype(value), decltype(rest)...> {
			return accept_type::operator()(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}

		constexpr auto operator()(auto&&... /*args*/) const -> Type {
			throw std::logic_error{"Type error"};
		}
};

// Instantiates `visit` or `accept` and automatically pass the `this` pointer as the first constructor argument.
template <class VisitOrAccept>
struct transfer_holder : VisitOrAccept {
	public:
		explicit constexpr transfer_holder(auto&&... args) :
				VisitOrAccept{
					this,
					std::forward<decltype(args)>(args)...
				} {}
};

// Select environment type for `accept` or `visit`. This is either `void` for stateless visitors, or
// something like `environment_of<T>` for runtime visitors.
template <class... Types>
struct select_transferee_environment;

template <class... Types>
using select_transferee_environment_t = select_transferee_environment<Types...>::type;

template <>
struct select_transferee_environment<> : std::type_identity<void> {};

template <class Type, class... Rest>
struct select_transferee_environment<Type, Rest...> : std::decay<Type> {};

// Transfer a JavaScript value from one domain to another
template <class Type, class Wrap, class... VisitArgs, class... AcceptArgs>
constexpr auto transfer_with(
	auto&& value,
	util::parameter_pack<VisitArgs...> /*visit_args_pack*/,
	std::type_identity_t<VisitArgs>... visit_args,
	util::parameter_pack<AcceptArgs...> /*accept_args_pack*/,
	std::type_identity_t<AcceptArgs>... accept_args
) -> decltype(auto) {
	using subject_type = std::decay_t<decltype(value)>;
	using target_type = std::decay_t<Type>;
	using visit_env = select_transferee_environment_t<VisitArgs...>;
	using accept_env = select_transferee_environment_t<AcceptArgs...>;
	using meta_holder = transferee_meta<Wrap, visit_env, subject_type, accept_env, target_type>;
	using visit_type = visit<meta_holder, subject_type>;
	using accept_type = Wrap::template accept<meta_holder, Type>;

	transfer_holder<visit_type> visit{std::forward<decltype(visit_args)>(visit_args)...};
	transfer_holder<accept_type> accept{std::forward<decltype(accept_args)>(accept_args)...};
	return visit(std::forward<decltype(value)>(value), accept);
}

// Transfer from known types, won't compile if all paths aren't known
export template <class Type, class... VisitArgs, class... AcceptArgs>
constexpr auto transfer_strict(auto&& value, std::tuple<VisitArgs...> visit_args, std::tuple<AcceptArgs...> accept_args) -> decltype(auto) {
	return std::invoke(
		[ & ]<std::size_t... VisitIndex, std::size_t... AcceptIndex>(
			std::index_sequence<VisitIndex...> /*visit_indices*/,
			std::index_sequence<AcceptIndex...> /*accept_indices*/
		) constexpr -> decltype(auto) {
			return transfer_with<Type, accept_pass>(
				std::forward<decltype(value)>(value),
				util::parameter_pack<VisitArgs...>{},
				std::get<VisitIndex>(std::forward<decltype(visit_args)>(visit_args))...,
				util::parameter_pack<AcceptArgs...>{},
				std::get<AcceptIndex>(std::forward<decltype(accept_args)>(accept_args))...
			);
		},
		std::make_index_sequence<sizeof...(VisitArgs)>{},
		std::make_index_sequence<sizeof...(AcceptArgs)>{}
	);
}

// Transfer "out" to a stateless acceptor. No tuple arguments needed.
export template <class Type>
constexpr auto transfer_out(auto&& value, auto&& visit_env, auto&&... visit_args) -> decltype(auto) {
	return transfer_with<Type, accept_with_throw>(
		std::forward<decltype(value)>(value),
		util::parameter_pack<decltype(visit_env), decltype(visit_args)...>{},
		std::forward<decltype(visit_env)>(visit_env),
		std::forward<decltype(visit_args)>(visit_args)...,
		util::parameter_pack<>{}
	);
}

export template <class Type>
constexpr auto transfer_out_strict(auto&& value, auto&& visit_env, auto&&... visit_args) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(
		std::forward<decltype(value)>(value),
		util::parameter_pack<decltype(visit_env), decltype(visit_args)...>{},
		std::forward<decltype(visit_env)>(visit_env),
		std::forward<decltype(visit_args)>(visit_args)...,
		util::parameter_pack<>{}
	);
}

// Transfer "in" from a stateless visitor.
export template <class Type>
constexpr auto transfer_in(auto&& value, auto&& accept_env, auto&&... accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_with_throw>(
		std::forward<decltype(value)>(value),
		util::parameter_pack<>{},
		util::parameter_pack<decltype(accept_env), decltype(accept_args)...>{},
		std::forward<decltype(accept_env)>(accept_env),
		std::forward<decltype(accept_args)>(accept_args)...
	);
}

export template <class Type>
constexpr auto transfer_in_strict(auto&& value, auto&& accept_env, auto&&... accept_args) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(
		std::forward<decltype(value)>(value),
		util::parameter_pack<>{},
		util::parameter_pack<decltype(accept_env), decltype(accept_args)...>{},
		std::forward<decltype(accept_env)>(accept_env),
		std::forward<decltype(accept_args)>(accept_args)...
	);
}

// Only used for testing. Stateless to stateless transfer
export template <class Type>
constexpr auto transfer(auto&& value) -> decltype(auto) {
	return transfer_with<Type, accept_with_throw>(std::forward<decltype(value)>(value), util::parameter_pack<>{}, util::parameter_pack<>{});
}

export template <class Type>
constexpr auto transfer_strict(auto&& value) -> decltype(auto) {
	return transfer_with<Type, accept_pass>(std::forward<decltype(value)>(value), util::parameter_pack<>{}, util::parameter_pack<>{});
}

// Allows the subject or target of `transfer` to pass through a value directly without invoking
// `visit` or `accept`. For example, as a directly created element of an array.
export template <class Type, class Tag = value_tag>
struct forward : util::pointer_facade {
	public:
		explicit forward(Type&& value) :
				value_{std::move(value)} {}
		explicit forward(const Type& value) :
				value_{value} {}

		constexpr auto operator->(this auto&& self) -> auto* { return &self.value_; }

	private:
		Type value_;
};

template <class Type>
forward(Type) -> forward<Type>;

template <class Type, class Tag>
struct accept<void, forward<Type, Tag>> {
		constexpr auto operator()(Tag /*tag*/, std::convertible_to<Type> auto&& value) const {
			return forward<Type, Tag>{std::forward<decltype(value)>(value)};
		}
};

template <class Type>
struct visit<void, forward<Type>> {
		constexpr auto operator()(auto&& value, const auto& /*accept*/) const {
			return *std::forward<decltype(value)>(value);
		}
};

} // namespace js
