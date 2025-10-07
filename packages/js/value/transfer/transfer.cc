module;
#include <cassert>
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
		template <class Accept>
		using accept = Accept;
};

// `Wrap` for `accept` which throws on unknown values
struct accept_with_throw {
		template <class Accept>
		struct accept_throw;
		template <class Accept>
		using accept = accept_throw<Accept>;
};

// Adds fallback acceptor which throws on unknown values
template <class Accept>
struct accept_with_throw::accept_throw : Accept {
		using accept_target_type = accept_target_t<Accept>;
		using Accept::Accept;

		using Accept::operator();
		constexpr auto operator()(auto_tag auto tag, const auto& visit, auto&& value) const -> accept_target_type
			requires(!std::invocable<Accept&, decltype(tag), decltype(visit), decltype(value)>) {
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
			auto visit_args,
			std::index_sequence<AcceptIndex...> /*accept_index*/,
			auto accept_args
		) :
				Visit{this, std::get<VisitIndex>(std::move(visit_args))...},
				Accept{this, std::get<AcceptIndex>(std::move(accept_args))...} {}

	public:
		// nb: Base class constructors do not perform return value optimization, so `util::elide` will
		// not work here.
		template <class... VisitArgs, class... AcceptArgs>
		explicit constexpr transfer_holder(
			std::tuple<VisitArgs...> visit_args,
			std::tuple<AcceptArgs...> accept_args
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

// Lookp up acceptor `accept_reference_type`
template <class Accept>
struct accept_reference_type : std::type_identity<void> {};

template <class Accept>
	requires requires { typename Accept::accept_reference_type; }
struct accept_reference_type<Accept> : std::type_identity<typename Accept::accept_reference_type> {};

// Transfer a JavaScript value from one domain to another
template <class Type, class Wrap, class... VisitArgs, class... AcceptArgs>
constexpr auto transfer_with(
	auto&& value,
	std::tuple<VisitArgs...> visit_args,
	std::tuple<AcceptArgs...> accept_args
) -> Type {
	using subject_type = std::decay_t<decltype(value)>;
	using target_type = std::decay_t<Type>;

	// compose accept type
	using accept_context_type = select_transferee_environment_t<std::remove_cvref_t<AcceptArgs>...>;
	using accept_meta_type = accept_meta_holder<
		Wrap,
		accept_context_type,
		accept_property_subject_t<typename visit_subject_for<subject_type>::type>>;
	using accept_type = accept_next<accept_meta_type, Type>;

	// compose visit type
	using visit_context_type = select_transferee_environment_t<std::remove_cvref_t<VisitArgs>...>;
	using visit_meta_type = visit_meta_holder<
		visit_context_type,
		typename visit_subject_for<subject_type>::type,
		accept_property_subject_t<typename accept_target_for<target_type>::type>,
		typename accept_reference_type<accept_type>::type>;
	using visit_type = visit<visit_meta_type, subject_type>;

	// cast subject to `T&&` or `const T&`. otherwise the `auto&&` parameters will accept too many
	// value categories.
	using subject_value_type = type_t<[]() consteval {
		using value_type = decltype(value);
		if constexpr (std::is_rvalue_reference_v<value_type>) {
			// `T&&`
			return type<std::remove_cvref_t<value_type>&&>;
		} else {
			// `const T&`
			return type<const std::remove_cvref_t<value_type>&>;
		}
	}()>;

	transfer_holder<visit_type, accept_type> visit_and_accept{std::move(visit_args), std::move(accept_args)};
	const visit_type& visit = visit_and_accept;
	accept_type& accept = visit_and_accept;
	return visit(subject_value_type(std::forward<decltype(value)>(value)), accept);
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
constexpr auto transfer(auto&& value, auto&& visit_args, auto&& accept_args) -> Type {
	return transfer_with<Type, accept_with_throw>(
		std::forward<decltype(value)>(value),
		std::forward<decltype(visit_args)>(visit_args),
		std::forward<decltype(accept_args)>(accept_args)
	);
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
// 	[](auto&& value, auto visit_args, auto accept_args) constexpr -> Type {
// 		return transfer_with<Type, Wrap>(
// 			std::forward<decltype(value)>(value),
// 			std::move(visit_args),
// 			std::move(accept_args)
// 		);
// 	},
// };

// export template <class Type>
// constexpr auto transfer = transfer_<Type, accept_with_throw>;

// export template <class Type>
// constexpr auto transfer_strict = transfer_<Type, accept_pass>;

} // namespace js
