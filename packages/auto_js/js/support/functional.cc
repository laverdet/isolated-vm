module;
#include <type_traits>
#include <utility>
export module auto_js:functional;
import util;

namespace js::functional {

// Thunk for most functions including free function, static functions, and constructors. The first
// `realm` parameter is optional for callbacks. This returns a function which always accepts
// `realm`.
export template <class Realm>
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
			[](Callback& callback, const Realm& /*realm*/, Args... args) noexcept(Nx) -> Result {
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
export template <class Realm>
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
			[](Callback& callback, That that, const Realm& /*realm*/, Args... args) noexcept(Nx) -> Result {
				return callback(that, std::forward<Args>(args)...);
			},
			std::move(callback),
		};
	};

	constexpr auto make = util::overloaded{make_with_realm, make_without_realm};
	return make(std::type_identity<util::function_signature_t<decltype(callback)>>{}, std::move(callback));
};

} // namespace js::functional
