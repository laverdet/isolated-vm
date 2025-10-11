module;
#include <functional>
#include <utility>
export module ivm.utility:functional;
import :tuple;
import :type_traits;

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

// Hide `-Wunused-value` warnings
export constexpr auto unused = [](auto&& /*nothing*/) -> void {};

// Invoke the given callable using an explicit and possibly shadowed `operator()()` on a base class,
// but without erasing the pointer type for deduced `this`. The syntax here is messy, and
// `InvokeAsType_` can be shadowed by the base class. Also, clang has some really spooky behavior
// which is resolved by breaking out invocations to deduced-this instances.
export template <class InvokeAsType_>
constexpr auto invoke_as(auto&& func, auto&&... args) -> decltype(auto) {
	return std::forward<decltype(func)>(func).InvokeAsType_::operator()(std::forward<decltype(args)>(args)...);
}

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
			const auto [... indices ] = util::sequence<sizeof...(Params)>;
			return invocable_(get<indices>(params_)..., std::forward<decltype(args)>(args)...);
		}

	private:
		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] flat_tuple<Params...> params_;
};

// Invoke the given function with the constant expression matching a runtime value.
export constexpr auto template_switch(const auto& value, auto case_pack, auto invoke) {
	auto dispatch = [ & ](const auto& dispatch, auto case_, auto... cases) -> decltype(auto) {
		if (value == case_) {
			return invoke(case_);
		} else {
			if constexpr (sizeof...(cases) == 0) {
				// default
				return invoke();
			} else {
				return dispatch(dispatch, cases...);
			}
		}
	};
	const auto [... cases ] = case_pack;
	return dispatch(dispatch, cases...);
}

} // namespace util
