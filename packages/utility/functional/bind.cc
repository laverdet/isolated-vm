module;
#include <type_traits>
#include <utility>
export module ivm.utility:functional.bind;
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

// rvalue (T&&) arguments are stored as values. lvalue (T&) arguments are stored as references.
template <class Invocable, class... Bound>
bind(Invocable, Bound&&...) -> bind<Invocable, remove_rvalue_reference_t<Bound>...>;

template <class Signature, std::size_t Count>
struct bind_signature;

template <class Signature, std::size_t Count>
using bind_signature_t = bind_signature<Signature, Count>::type;

template <class Invocable, class Signature, class... Bound>
class bind_with;

template <class Invocable, class... Bound>
using bind_with_params_t =
	bind_with<Invocable, bind_signature_t<function_signature_t<Invocable>, sizeof...(Bound)>, Bound...>;

// It requires `function_signature_t` but there's nothing saying that an overloaded invocable
// couldn't be passed. It would just be a different implementation.
template <class Invocable, class... Bound>
class bind : public bind_with_params_t<Invocable, Bound...> {
		using bind_with_params_t<Invocable, Bound...>::bind_with_params_t;
};

// Storage for `bind_with` specializations
template <class Invocable, class... Bound>
struct bind_storage {
	public:
		explicit constexpr bind_storage()
			requires std::is_empty_v<Invocable> && (... && std::is_empty_v<Bound>)
		= default;

		explicit constexpr bind_storage(Invocable invocable, Bound... params) :
				invocable_{std::move(invocable)},
				params_{std::forward<Bound>(params)...} {}

		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] flat_tuple<Bound...> params_;
};

// Explicit signature `util::bind`
template <class Invocable, class... Args, bool Nx, class Result, class... Bound>
class bind_with<Invocable, auto(Args...) noexcept(Nx)->Result, Bound...> : private bind_storage<Invocable, Bound...> {
	private:
		using storage_type = bind_storage<Invocable, Bound...>;
		using try_cvref_type = apply_cvref_t<ensure_reference_t<invocable_type_t<Invocable>>, bind_with>;

		template <class Type>
		using cvref_param_type =
			std::conditional_t<std::is_reference_v<Type>, Type, apply_cvref_t<try_cvref_type, std::remove_reference_t<Type>>>;

		using this_type =
			std::conditional_t<std::is_invocable_v<try_cvref_type, cvref_param_type<Bound>...>, try_cvref_type, bind_with&>;
		constexpr static auto invoke_as_mutable = std::is_same_v<this_type, bind_with&>;
		constexpr static auto invoke_as_forward = !invoke_as_mutable;

	public:
		using storage_type::storage_type;

		// This overload allows `std::invocable<bind_with<...>>` without reference, which makes concept
		// parameters work correctly.
		constexpr auto operator()(Args /*&&*/... args) noexcept(Nx) -> Result
			requires invoke_as_mutable {
			return invoke(*this, std::forward<Args>(args)...);
		}

		// Otherwise, explicit `this` is used
		constexpr auto operator()(this this_type self, Args /*&&*/... args) noexcept(Nx) -> Result
			requires invoke_as_forward {
			return invoke(std::forward<this_type>(self), std::forward<Args>(args)...);
		}

	private:
		constexpr static auto invoke(auto&& self, Args /*&&*/... args) noexcept(Nx) -> Result {
			const auto [... indices ] = util::sequence<sizeof...(Bound)>;
			return std::forward<this_type>(self).invocable_(
				get<indices>(util::forward_from<this_type>(self.params_))...,
				std::forward<decltype(args)>(args)...
			);
		}

		using storage_type::invocable_;
		using storage_type::params_;
};

// Strip the first N parameters from the signature
template <class First, class... Args, bool Nx, class Result, std::size_t Count>
	requires(Count > 0)
struct bind_signature<auto(First, Args...) noexcept(Nx)->Result, Count>
		: bind_signature<auto(Args...) noexcept(Nx)->Result, Count - 1> {};

template <class... Args, bool Nx, class Result>
struct bind_signature<auto(Args...) noexcept(Nx)->Result, 0>
		: std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

} // namespace util
