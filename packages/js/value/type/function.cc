module;
#include <string_view>
#include <utility>
export module isolated_js:function;
import :tag;
import :visit;
import ivm.utility;

namespace js {

// Holds `this` and arguments vector
export struct arguments_tag : vector_tag {};

// Non-constructor, non-member function
export template <class Callback>
struct free_function {
		explicit constexpr free_function(Callback callback) :
				callback{std::move(callback)} {}

		Callback callback;
		constexpr static auto name = std::string_view{};
};

template <class Function>
struct visit<void, free_function<Function>> {
		constexpr auto operator()(auto&& function, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, function_tag{}, *this, std::forward<decltype(function)>(function));
		}
};

// Convenience maker for plain function
export template <auto Callback>
consteval auto make_static_function() {
	return js::free_function{util::invocable_constant<Callback>{}};
}

} // namespace js
