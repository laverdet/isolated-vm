module;
#include <string_view>
#include <utility>
export module isolated_js.function;
import isolated_js.tag;
import isolated_js.visit;
import ivm.utility;

namespace js {

// Holds `this` and arguments vector
export struct arguments_tag : vector_tag {};

// Non-constructor, non-member stateless free function
export template <auto Function>
struct free_function {
		constexpr static auto invocable = Function;
		constexpr static auto name = std::string_view{};
};

template <auto Function>
struct visit<void, free_function<Function>> {
		constexpr auto operator()(const auto& function, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, function);
		}
};

// Function which requires carried state
export template <class Invocable>
struct bound_function {
		explicit constexpr bound_function(Invocable invocable) :
				invocable{std::move(invocable)} {}

		Invocable invocable;
		constexpr static auto name = std::string_view{};
};

template <class Function>
struct visit<void, bound_function<Function>> {
		constexpr auto operator()(auto&& function, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, std::forward<decltype(function)>(function));
		}
};

} // namespace js
