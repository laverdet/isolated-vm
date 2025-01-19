module;
#include <utility>
export module isolated_js.function;
import isolated_js.tag;
import isolated_js.visit;
import ivm.utility;

namespace js {

// Holds `this` and arguments vector
export struct arguments_tag : vector_tag {};

// Function which is statically invocable
export template <auto Function, class Signature = util::function_signature_t<decltype(Function)>>
struct free_function;

template <auto Function, class Result, class... Args>
struct free_function<Function, Result(Args...)> {};

template <auto Function, class Signature>
struct visit<void, free_function<Function, Signature>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, std::forward<decltype(value)>(value));
		}
};

// Function which requires state
export template <class Invocable, class Signature>
class bound_function;

template <class Invocable, class Result, class... Args>
class bound_function<Invocable, Result(Args...)> {
	public:
		explicit bound_function(Invocable functor) :
				invocable_{std::move(functor)} {}

		auto operator()(this auto& self, Args... args) -> Result {
			return self.invocable_(std::forward<Args>(args)...);
		}

	private:
		Invocable invocable_;
};

template <class Invocable>
bound_function(Invocable) -> bound_function<Invocable, util::function_signature_t<Invocable>>;

template <class Invocable, class Signature>
struct visit<void, bound_function<Invocable, Signature>> {
		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, std::forward<decltype(value)>(value));
		}
};

} // namespace js
