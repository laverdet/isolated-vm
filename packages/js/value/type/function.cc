module;
#include <string_view>
#include <utility>
export module isolated_js:function;
import :tag;
import :visit;

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
		template <class Accept>
		constexpr auto operator()(auto&& function, const Accept& accept) const -> accept_target_t<Accept> {
			return accept(function_tag{}, *this, std::forward<decltype(function)>(function));
		}
};

} // namespace js
