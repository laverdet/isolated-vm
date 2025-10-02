module;
#include <functional>
#include <utility>
export module ivm.utility:functional;
import :tuple;

namespace util {

// Holder for functions in libc++ which doesn't support std::move_only_function
#if defined(__cpp_lib_move_only_function) && !defined(DEBUG)

export template <class Signature>
using maybe_move_only_function = std::move_only_function<Signature>;

export template <class Fn>
auto make_indirect_moveable_function(Fn fn) {
	return fn;
}

#else

export template <class Signature>
using maybe_move_only_function = std::function<Signature>;

export template <class Fn>
auto make_indirect_moveable_function(Fn&& fn) {
	auto fn_ptr = std::make_shared<std::decay_t<Fn>>(std::forward<Fn>(fn));
	return [ = ](auto&&... args) -> decltype(auto) {
		return (*fn_ptr)(std::forward<decltype(args)>(args)...);
	};
}

#endif

// `constructor<T>` adapts a constructible type into a callable object
template <class Type>
struct constructor_t {
		constexpr auto operator()(auto&&... args) const -> Type
			requires std::constructible_from<Type, decltype(args)...> {
			return Type(std::forward<decltype(args)>(args)...);
		}
};

export template <class Type>
constexpr inline auto constructor = constructor_t<Type>{};

// Captures the given parameters in way such that empty objects yield an empty invocable. See:
//   static_assert(std::is_empty_v<decltype([]() {})>);
//   static_assert(!std::is_empty_v<decltype([ a = std::monostate{} ]() {})>);
export template <class Invocable, class... Params>
class bind_parameters;

template <class Invocable, class... Params>
class bind_parameters {
	public:
		bind_parameters() = default;
		constexpr explicit bind_parameters(Invocable invocable, Params... params) :
				invocable_{std::move(invocable)},
				params_{std::move(params)...} {}

		constexpr auto operator()(auto&&... args) -> decltype(auto)
			requires std::invocable<Invocable&, Params&..., decltype(args)...> {
			return std::invoke(
				[ & ]<size_t... Index>(std::index_sequence<Index...> /*indices*/) constexpr -> decltype(auto) {
					return invocable_(get<Index>(params_)..., std::forward<decltype(args)>(args)...);
				},
				std::make_index_sequence<sizeof...(Params)>{}
			);
		}

	private:
		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] flat_tuple<Params...> params_;
};

} // namespace util
