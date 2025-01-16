module;
#include <functional>
#include <utility>
export module isolated_js.function;
import isolated_js.tag;
import isolated_js.visit;

namespace js {

// Function which is statically invocable
export template <auto Fn>
struct free_function {};

// Function which requires state
export template <class Functor, class Type>
class bound_function;

template <class Functor, class Result, class... Args>
class bound_function<Functor, std::function<Result(Args...)>> {
	public:
		explicit bound_function(Functor functor) :
				functor_{std::move(functor)} {}

		auto operator()(Args... args) const -> Result {
			return functor_(std::forward<Args>(args)...);
		}

	private:
		Functor functor_;
};

template <class Functor>
bound_function(Functor) -> bound_function<Functor, decltype(std::function{std::declval<Functor>()})>;

// Visitor for functions
template <class Functor, class Type>
struct visit<void, bound_function<Functor, Type>> : visit<void, void> {
		using visit<void, void>::visit;

		constexpr auto operator()(auto&& value, const auto& accept) const -> decltype(auto) {
			return accept(function_tag{}, std::forward<decltype(value)>(value));
		}
};

} // namespace js
