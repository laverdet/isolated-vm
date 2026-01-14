module;
#include "shim/macro.h"
#include <type_traits>
#include <utility>
export module util:functional.bind;
import :functional.flat_tuple;
import :type_traits;
import :utility;

namespace util {

// Captures the given parameters in way such that empty objects yield an empty and
// default-constructible invocable. See:
//   static_assert(std::is_empty_v<decltype([]() {})>);
//   static_assert(!std::is_empty_v<decltype([ a = std::monostate{} ]() {})>);
export template <class Invocable, class... Bound>
class bind;

template <class Invocable, class... Bound>
bind(Invocable, Bound...) -> bind<Invocable, Bound...>;

// Storage for `bind` specializations
template <class Invocable, class... Bound>
struct bind_storage {
	public:
		explicit constexpr bind_storage()
			requires std::is_empty_v<Invocable> && (... && std::is_empty_v<Bound>)
		= default;

		explicit constexpr bind_storage(Invocable invocable, Bound... params) :
				invocable_{std::move(invocable)},
				params_{std::forward<Bound>(params)...} {}

		NO_UNIQUE_ADDRESS Invocable invocable_;
		NO_UNIQUE_ADDRESS flat_tuple<Bound...> params_;
};

// Overloaded signature `util::bind`
template <class Invocable, class... Bound>
class bind_overloaded : private bind_storage<Invocable, Bound...> {
	private:
		using storage_type = bind_storage<Invocable, Bound...>;
		using storage_type::invocable_;
		using storage_type::params_;

	public:
		using storage_type::storage_type;

		template <class... Args>
		constexpr auto operator()(Args&&... args) noexcept(std::is_nothrow_invocable_v<Invocable&, Bound&..., Args...>)
			-> std::invoke_result_t<Invocable&, Bound&..., Args...> {
			return invoke(std::forward<Args>(args)...);
		}

		template <class Self, class... Args>
		constexpr auto operator()(this Self&& self, Args&&... args) noexcept(std::is_nothrow_invocable_v<apply_cvref_t<Self, Invocable>, apply_cvref_t<Self, Bound>..., Args...>)
			-> std::invoke_result_t<apply_cvref_t<Self, Invocable>, apply_cvref_t<Self, Bound>..., Args...> {
			return std::forward<Self>(self).invoke(std::forward<Args>(args)...);
		}

	private:
		constexpr auto invoke(this auto&& self, auto&&... args) -> decltype(auto) {
			const auto [... indices ] = util::sequence<sizeof...(Bound)>;
			return std::forward<decltype(self)>(self).invocable_(
				get<indices>(std::forward<decltype(self)>(self).params_)...,
				std::forward<decltype(args)>(args)...
			);
		}
};

// Explicit signature `util::bind`
template <class Invocable, class Signature, class... Bound>
class bind_explicit;

template <class Invocable, class... Args, bool Nx, class Result, class... Bound>
class bind_explicit<Invocable, auto(Args...) noexcept(Nx)->Result, Bound...> : private bind_storage<Invocable, Bound...> {
	private:
		using storage_type = bind_storage<Invocable, Bound...>;
		using storage_type::invocable_;
		using storage_type::params_;

		using try_cvref_type = apply_cvref_t<ensure_reference_t<invocable_type_t<Invocable>>, Invocable>;
		using cvref_type =
			std::conditional_t<std::is_invocable_v<try_cvref_type, apply_cvref_t<try_cvref_type, Bound>...>, try_cvref_type, bind_explicit&>;
		using this_type = apply_cvref_t<cvref_type, bind_explicit>;
		constexpr static auto invoke_as_mutable = std::is_same_v<this_type, bind_explicit&>;
		constexpr static auto invoke_as_forward = !invoke_as_mutable;

	public:
		using storage_type::storage_type;

		// This overload allows `std::invocable<bind<...>>` without a reference, which makes concept
		// parameters work correctly.
		constexpr auto operator()(Args /*&&*/... args) noexcept(Nx) -> Result
			requires invoke_as_mutable {
			return invoke(std::forward<Args>(args)...);
		}

		// Otherwise, explicit `this` is used
		constexpr auto operator()(this this_type self, Args /*&&*/... args) noexcept(Nx) -> Result
			requires invoke_as_forward {
			return std::forward<this_type>(self).invoke(std::forward<Args>(args)...);
		}

	private:
		constexpr auto invoke(this auto&& self, Args /*&&*/... args) noexcept(Nx) -> Result {
			const auto [... indices ] = util::sequence<sizeof...(Bound)>;
			return std::forward<this_type>(self).invocable_(
				get<indices>(std::forward<this_type>(self).params_)...,
				std::forward<decltype(args)>(args)...
			);
		}
};

// Strip the first N parameters from the signature
template <class Signature, std::size_t Count>
struct bind_signature;

template <class Signature, std::size_t Count>
using bind_signature_t = bind_signature<Signature, Count>::type;

template <class First, class... Args, bool Nx, class Result, std::size_t Count>
	requires(Count > 0)
struct bind_signature<auto(First, Args...) noexcept(Nx)->Result, Count>
		: bind_signature<auto(Args...) noexcept(Nx)->Result, Count - 1> {};

template <class... Args, bool Nx, class Result>
struct bind_signature<auto(Args...) noexcept(Nx)->Result, 0>
		: std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

template <class Invocable, class... Bound>
using bind_explicit_params_t =
	bind_explicit<Invocable, bind_signature_t<function_signature_t<Invocable>, sizeof...(Bound)>, Bound...>;

// Instantiate without known function signature
template <class Invocable, class... Bound>
class bind : public bind_overloaded<Invocable, Bound...> {
		using bind_overloaded<Invocable, Bound...>::bind_overloaded;
};

// Instantiate with known function signature
template <class Invocable, class... Bound>
	requires requires { typename function_signature_t<Invocable>; }
class bind<Invocable, Bound...> : public bind_explicit_params_t<Invocable, Bound...> {
		using bind_explicit_params_t<Invocable, Bound...>::bind_explicit_params_t;
};
} // namespace util
