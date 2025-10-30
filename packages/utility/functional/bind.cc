module;
#include <type_traits>
#include <utility>
export module ivm.utility:functional.bind;
import :type_traits;
import :utility;
import :tuple;

namespace util {

// Captures the given parameters in way such that empty objects yield an empty invocable. See:
//   static_assert(std::is_empty_v<decltype([]() {})>);
//   static_assert(!std::is_empty_v<decltype([ a = std::monostate{} ]() {})>);
export template <class Invocable, class... Bound>
class bind;

// rvalue (T&&) arguments are stored as values. lvalue (T&) arguments are stored as references.
template <class Invocable, class... Bound>
bind(Invocable, Bound&&...) -> bind<Invocable, util::remove_rvalue_reference_t<Bound>...>;

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
class bind_storage {
	public:
		bind_storage() = default;
		constexpr explicit bind_storage(Invocable invocable, Bound... params) :
				invocable_{std::move(invocable)},
				params_{std::forward<Bound>(params)...} {}

	private:
		template <class, class, class...>
		friend class bind_with;

		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] flat_tuple<Bound...> params_;
};

// Const version
template <class Invocable, class... Args, bool Nx, class Result, class... Bound>
class bind_with<Invocable, auto(Args...) noexcept(Nx)->Result, Bound...> : bind_storage<Invocable, Bound...> {
	private:
		using storage_type = bind_storage<Invocable, Bound...>;
		using storage_type::invocable_;
		using storage_type::params_;

	public:
		using storage_type::storage_type;

		constexpr auto operator()(Args... args) const noexcept(Nx) -> Result {
			const auto [... indices ] = util::sequence<sizeof...(Bound)>;
			return invocable_(get<indices>(params_)..., std::forward<decltype(args)>(args)...);
		}
};

// Mutable version
template <class Invocable, class... Args, bool Nx, class Result, class... Bound>
	requires std::invocable<Invocable&, Bound&..., Args...>
class bind_with<Invocable, auto(Args...) noexcept(Nx)->Result, Bound...> : bind_storage<Invocable, Bound...> {
	private:
		using storage_type = bind_storage<Invocable, Bound...>;
		using storage_type::invocable_;
		using storage_type::params_;

	public:
		using storage_type::storage_type;

		constexpr auto operator()(Args... args) noexcept(Nx) -> Result {
			const auto [... indices ] = util::sequence<sizeof...(Bound)>;
			return invocable_(get<indices>(params_)..., std::forward<decltype(args)>(args)...);
		}
};

// Strip the first N parameters from the signature
template <class First, class... Args, bool Nx, class Result, std::size_t Count>
	requires(Count > 0)
struct bind_signature<auto(First, Args...) noexcept(Nx)->Result, Count> : bind_signature<auto(Args...) noexcept(Nx)->Result, Count - 1> {};

template <class... Args, bool Nx, class Result>
struct bind_signature<auto(Args...) noexcept(Nx)->Result, 0> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

} // namespace util
