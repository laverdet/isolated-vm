module;
#include <tuple>
#include <utility>
export module util:functional.elide;

namespace util {

// Lazy value creation. Enables construction of non-movable types via copy elision. Also, can be
// used for deferred construction for `try_emplace`.

// Adapted from discontinued proposal:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3288r0.html
export template <class Type, class Invocable, class... Args>
class elide {
	public:
		using result_type = Type;

		explicit constexpr elide(Invocable invocable, Args /*&&*/... args) :
				invocable_{std::move(invocable)},
				args_{std::forward<decltype(args)>(args)...} {}

		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator result_type() && {
			auto [... args ] = std::move(args_);
			return std::move(invocable_)(std::move(args)...);
		}

	private:
		[[no_unique_address]] Invocable invocable_;
		[[no_unique_address]] std::tuple<Args...> args_;
};

template <class Invocable, class... Args>
elide(Invocable, Args&&...) -> elide<std::invoke_result_t<Invocable, Args...>, Invocable, Args...>;

// `util::constructor<T>` adapts a constructible type into a callable object. Great for use with
// `util::elide`.
template <class Type>
struct constructor_t {
		constexpr auto operator()(auto&&... args) const -> Type
			requires std::constructible_from<Type, decltype(args)...> {
			return Type(std::forward<decltype(args)>(args)...);
		}
};

export template <class Type>
constexpr inline auto constructor = constructor_t<Type>{};

} // namespace util
