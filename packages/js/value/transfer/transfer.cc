module;
#include <concepts>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module isolated_js:transfer;
export import :accept;
export import :tag;
export import :transfer.types;
export import :visit;
import ivm.utility;

namespace js {

// Default `accept` passthrough `Wrap`
struct accept_pass {
		template <class Meta, class Type>
		using accept = js::accept<Meta, Type>;
};

// `Wrap` for `accept` which throws on unknown values
struct accept_with_throw {
		template <class Meta, class Type>
		struct accept_throw;
		template <class Meta, class Type>
		using accept = accept_throw<Meta, Type>;
};

// Adds fallback acceptor which throws on unknown values
template <class Meta, class Type>
struct accept_with_throw::accept_throw : js::accept<Meta, Type> {
		using accept_target_type = Type;
		using accept_type = js::accept<Meta, Type>;
		using accept_type::accept_type;

		using accept_type::operator();
		constexpr auto operator()(auto_tag auto tag, const auto& visit, auto&& value) const -> accept_target_type
			requires(!std::invocable<accept_type&, decltype(tag), decltype(visit), decltype(value)>) {
			throw std::logic_error{"Type error"};
		}
};

// Instantiates `visit` and `accept` and automatically pass the `this` pointer as the first
// constructor argument.
template <class Visit, class Accept>
struct transfer_holder : Visit, Accept {
	private:
		template <std::size_t... VisitIndex, std::size_t... AcceptIndex>
		explicit constexpr transfer_holder(
			std::index_sequence<VisitIndex...> /*visit_index*/,
			auto&& visit_args,
			std::index_sequence<AcceptIndex...> /*accept_index*/,
			auto&& accept_args
		) :
				Visit{this, std::get<VisitIndex>(std::forward<decltype(visit_args)>(visit_args))...},
				Accept{this, std::get<AcceptIndex>(std::forward<decltype(accept_args)>(accept_args))...} {}

	public:
		// nb: Base class constructors do not perform return value optimization, so `util::elide` will
		// not work here.
		template <class... VisitArgs, class... AcceptArgs>
		explicit constexpr transfer_holder(
			std::tuple<VisitArgs...>&& visit_args,
			std::tuple<AcceptArgs...>&& accept_args
		) :
				transfer_holder{
					std::make_index_sequence<sizeof...(VisitArgs)>{},
					std::move(visit_args),
					std::make_index_sequence<sizeof...(AcceptArgs)>{},
					std::move(accept_args),
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
	std::tuple<VisitArgs...>&& visit_args,
	std::tuple<AcceptArgs...>&& accept_args
) -> Type {
	using subject_type = std::decay_t<decltype(value)>;
	using target_type = std::decay_t<Type>;
	using visit_env = select_transferee_environment_t<std::remove_cvref_t<VisitArgs>...>;
	using accept_env = select_transferee_environment_t<std::remove_cvref_t<AcceptArgs>...>;
	using meta_holder = transferee_meta<Wrap, visit_env, subject_type, accept_env, target_type>;
	using visit_type = visit<meta_holder, subject_type>;
	using accept_type = Wrap::template accept<meta_holder, Type>;

	transfer_holder<visit_type, accept_type> visit_and_accept{std::move(visit_args), std::move(accept_args)};
	const visit_type& visit = visit_and_accept;
	accept_type& accept = visit_and_accept;
	return visit(std::forward<decltype(value)>(value), accept);
}

// Transfer "out" to a stateless acceptor.
template <class Type, class Wrap>
constexpr auto transfer_out_ = [](auto&& value, auto&& visit_env, auto&&... visit_args) constexpr -> Type {
	return transfer_with<Type, Wrap>(
		std::forward<decltype(value)>(value),
		std::forward_as_tuple(
			std::forward<decltype(visit_env)>(visit_env),
			std::forward<decltype(visit_args)>(visit_args)...
		),
		std::tuple{}
	);
};

export template <class Type>
constexpr auto transfer_out = transfer_out_<Type, accept_with_throw>;

export template <class Type>
constexpr auto transfer_out_strict = transfer_out_<Type, accept_pass>;

// Transfer "in" from a stateless visitor.
template <class Type, class Wrap>
constexpr auto transfer_in_ = [](auto&& value, auto&& accept_env, auto&&... accept_args) constexpr -> Type {
	return transfer_with<Type, Wrap>(
		std::forward<decltype(value)>(value),
		std::tuple{},
		std::forward_as_tuple(
			std::forward<decltype(accept_env)>(accept_env),
			std::forward<decltype(accept_args)>(accept_args)...
		)
	);
};

export template <class Type>
constexpr auto transfer_in = transfer_in_<Type, accept_with_throw>;

export template <class Type>
constexpr auto transfer_in_strict = transfer_in_<Type, accept_pass>;

// Transfer from one non-void context to another.
export template <class Type>
constexpr auto transfer(auto&& value) -> Type {
	return transfer_with<Type, accept_with_throw>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
}

export template <class Type>
constexpr auto transfer_strict(auto&& value) -> Type {
	return transfer_with<Type, accept_pass>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
}

export template <class Type>
constexpr auto transfer_strict(auto&& value, auto&& visit_args, auto&& accept_args) -> Type {
	return transfer_with<Type, accept_pass>(
		std::forward<decltype(value)>(value),
		std::forward<decltype(visit_args)>(visit_args),
		std::forward<decltype(accept_args)>(accept_args)
	);
}

// Another unexplainable clang bug?
// call to immediate function '...' is not a constant expression
// template <class Type, class Wrap>
// constexpr auto transfer_ = util::overloaded{
// 	[](auto&& value) constexpr -> Type {
// 		return transfer_with<Type, Wrap>(std::forward<decltype(value)>(value), std::tuple{}, std::tuple{});
// 	},
// 	[](auto&& value, auto&& visit_args, auto&& accept_args) constexpr -> Type {
// 		return transfer_with<Type, Wrap>(
// 			std::forward<decltype(value)>(value),
// 			std::forward<decltype(visit_args)>(visit_args),
// 			std::forward<decltype(accept_args)>(accept_args)
// 		);
// 	},
// };

// export template <class Type>
// constexpr auto transfer = transfer_<Type, accept_with_throw>;

// export template <class Type>
// constexpr auto transfer_strict = transfer_<Type, accept_pass>;

// Allows the subject or target of `transfer` to pass through a value directly without invoking
// `visit` or `accept`. For example, as a directly created element of an array.
export template <class Type, class Tag = value_tag>
struct forward : util::pointer_facade {
	public:
		explicit forward(const Type& value) :
				value_{value} {}
		explicit forward(Type&& value) :
				value_{std::move(value)} {}

		constexpr auto operator->(this auto&& self) -> auto* { return &self.value_; }

	private:
		Type value_;
};

template <class Type>
forward(Type) -> forward<Type>;

template <class Type, class Tag>
struct accept<void, forward<Type, Tag>> {
		constexpr auto operator()(Tag /*tag*/, visit_holder /*visit*/, std::convertible_to<Type> auto&& value) const -> forward<Type, Tag> {
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
