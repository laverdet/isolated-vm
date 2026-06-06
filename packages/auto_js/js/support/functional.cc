export module auto_js:functional;
import :transfer.types;
import std;
import util;

namespace js::functional {

// Thunk for most functions including free function, static functions, and constructors. The first
// `realm` parameter is optional for callbacks. This returns a function which always accepts
// `realm`.
export template <class Realm, class Nil = Realm>
constexpr auto thunk_free_function = []<class Callback>(Callback callback) constexpr -> auto {
	constexpr auto make_with_realm =
		[]<class Type, class... Args, bool Nx, class Result>(
			std::type_identity<auto(Type, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto
		requires std::is_convertible_v<Realm, Type> {
			return std::move(callback);
		};
	constexpr auto make_without_realm =
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		return util::bind{
			[](Callback& callback, Nil /*realm*/, Args... args) noexcept(Nx) -> Result {
				return callback(std::forward<Args>(args)...);
			},
			std::move(callback),
		};
	};

	constexpr auto make = util::overloaded{make_with_realm, make_without_realm};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

// This takes the place of `thunk_free_function` for member functions, where `realm` is the second
// parameter.
export template <class Realm, class Nil = Realm>
constexpr auto thunk_member_function = []<class Callback>(Callback callback) constexpr -> auto {
	constexpr auto make_with_realm =
		[]<class That, class Type, class... Args, bool Nx, class Result>(
			std::type_identity<auto(That, Type, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto
		requires std::is_convertible_v<Realm, Type> {
			return std::move(callback);
		};
	constexpr auto make_without_realm =
		[]<class That, class... Args, bool Nx, class Result>(
			std::type_identity<auto(That, Args...) noexcept(Nx)->Result> /*signature*/,
			auto callback
		) -> auto {
		return util::bind{
			[](Callback& callback, That that, Nil /*realm*/, Args... args) noexcept(Nx) -> Result {
				return callback(that, std::forward<Args>(args)...);
			},
			std::move(callback),
		};
	};

	constexpr auto make = util::overloaded{make_with_realm, make_without_realm};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

// Given a function parameter type, resolve the argument type
export template <class Type>
struct parameter_transfer_as;

export template <class Type>
using parameter_transfer_as_t = typename parameter_transfer_as<Type>::type;

template <class Type>
struct parameter_transfer_as : std::type_identity<Type> {
		static_assert(!std::is_reference_v<Type>, "parameter type must be T or const T&, not T&");
};

template <class Type>
struct parameter_transfer_as<const Type&> : parameter_transfer_as<Type> {};

template <class Type>
	requires requires { typename transfer_type_t<Type>; }
struct parameter_transfer_as<Type> : transfer_type<Type> {};

template <class Type>
	requires requires { typename transfer_type_t<Type>; }
struct parameter_transfer_as<Type&> : transfer_type<Type> {};

} // namespace js::functional
