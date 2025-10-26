module;
#include <utility>
export module ivm.utility:functional.bind;
import :type_traits;
import :utility;
import :tuple;

namespace util {

// Captures the given parameters in way such that empty objects yield an empty invocable. See:
//   static_assert(std::is_empty_v<decltype([]() {})>);
//   static_assert(!std::is_empty_v<decltype([ a = std::monostate{} ]() {})>);
export template <class Invocable, class... Params>
class bind;

template <class Invocable, class... Params>
bind(Invocable, Params...) -> bind<Invocable, Params...>;

template <class Signature, std::size_t Count>
struct bind_signature;

template <class Signature, std::size_t Count>
using bind_signature_t = bind_signature<Signature, Count>::type;

template <class Invocable, class Signature, class... Params>
class bind_with;

template <class Invocable, class... Params>
using bind_with_params_t =
	bind_with<Invocable, bind_signature_t<function_signature_t<Invocable>, sizeof...(Params)>, Params...>;

// It requires `function_signature_t` but there's nothing saying that an overloaded invocable
// couldn't be passed. It would just be a different implementation.
template <class Invocable, class... Params>
class bind : public bind_with_params_t<Invocable, Params...> {
		using bind_with_params_t<Invocable, Params...>::bind_with_params_t;
};

template <class Invocable, class... Args, bool Nx, class Result, class... Params>
class bind_with<Invocable, auto(Args...) noexcept(Nx)->Result, Params...> {
	public:
		constexpr explicit bind_with(Invocable invocable, Params... params) :
				invocable_{std::move(invocable)},
				params_{std::move(params)...} {}

		constexpr auto operator()(Args... args) noexcept(Nx) -> Result {
			const auto [... indices ] = util::sequence<sizeof...(Params)>;
			return invocable_(get<indices>(params_)..., std::forward<decltype(args)>(args)...);
		}

	private:
		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] flat_tuple<Params...> params_;
};

// Strip the first N parameters from the signature
template <class First, class... Args, bool Nx, class Result, std::size_t Count>
struct bind_signature<auto(First, Args...) noexcept(Nx)->Result, Count> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

template <class... Args, bool Nx, class Result>
struct bind_signature<auto(Args...) noexcept(Nx)->Result, 0> : std::type_identity<auto(Args...) noexcept(Nx)->Result> {};

} // namespace util
