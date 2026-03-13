module;
#include <functional>
#include <utility>
export module util:functional;
export import :functional.bind;
export import :functional.elide;
export import :functional.function_constant;
export import :functional.function_ref;
export import :functional.regular_return;
import :type_traits;

namespace util {

// https://en.cppreference.com/w/cpp/utility/variant/visit
export template <class... Visitors>
struct overloaded : Visitors... {
		using Visitors::operator()...;
};

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
using maybe_move_only_function = std::function<remove_function_cvref_t<Signature>>;

export template <class Fn>
auto make_indirect_moveable_function(Fn&& fn) {
	auto fn_ptr = std::make_shared<std::remove_cvref_t<Fn>>(std::forward<Fn>(fn));
	return [ = ](auto&&... args) -> decltype(auto) {
		return (*fn_ptr)(std::forward<decltype(args)>(args)...);
	};
}

#endif

// Hide `-Wunused-value` warnings
export constexpr auto unused = [](auto&& /*nothing*/) -> void {};

// Invoke the given callable as a base class of itself. cvref qualifiers are forwarded, so `Type` is
// expected to be unqualified.
export template <class Type, class From>
constexpr auto invoke_as(From&& func, auto&&... args)
	-> decltype(std::declval<apply_cvref_t<From, Type>>()(std::declval<decltype(args)>()...)) {
	return static_cast<apply_cvref_t<From, Type>>(std::forward<decltype(func)>(func))(std::forward<decltype(args)>(args)...);
}

// Invoke the given callable using an explicit and possibly shadowed `operator()()` on a base class,
// but without erasing the pointer type for deduced `this`. The syntax here is messy, and
// `InvokeAsType_` can be shadowed by the base class. Also, clang has some really spooky behavior
// which is resolved by breaking out invocations to deduced-this instances.
export template <class InvokeAsType_>
constexpr auto invoke_this_as(auto&& func, auto&&... args)
	// This is functionally the same as ` -> decltype(auto)` but works around the error:
	// 'invoke_as<...>' with deduced return type cannot be used before it is defined
	-> decltype(std::declval<decltype(func)>().InvokeAsType_::operator()(std::declval<decltype(args)>()...)) {
	return std::forward<decltype(func)>(func).InvokeAsType_::operator()(std::forward<decltype(args)>(args)...);
}

// Invoke the given function with the constant expression matching a runtime value.
export constexpr auto template_switch(const auto& value, auto case_pack, auto invoke) -> decltype(auto) {
	auto dispatch = [ & ](this const auto& self, auto case_, auto... cases) -> decltype(auto) {
		if (value == case_) {
			return invoke(case_);
		} else {
			if constexpr (sizeof...(cases) == 0) {
				// default
				return invoke();
			} else {
				return self(cases...);
			}
		}
	};
	const auto [... cases ] = case_pack;
	// NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
	return dispatch(cases...);
}

// libc++'s `std::reference_wrapper` causes a template instantiation while looking for
// `result_type`. That breaks our transfer machinery.
export template <class Type>
class reference_wrapper {
	public:
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr reference_wrapper(Type& value) : value_{&value} {}

		constexpr auto operator()(auto&&... args) const
			-> decltype(std::declval<Type&>()(std::declval<decltype(args)>()...)) {
			return (*value_)(std::forward<decltype(args)>(args)...);
		}

		constexpr auto get() const -> Type& { return *value_; }

	private:
		Type* value_;
};

} // namespace util
