module;
#include <concepts>
#include <utility>
#include <variant>
export module ivm.utility:utility;
import :type_traits;

namespace util {

// https://en.cppreference.com/w/cpp/utility/variant/visit
export template <class... Visitors>
struct overloaded : Visitors... {
		using Visitors::operator()...;
};

// `boost::noncopyable` actually prevents moving too
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
export class non_copyable {
	protected:
		non_copyable() = default;

	public:
		non_copyable(non_copyable&&) = default;
		auto operator=(non_copyable&&) -> non_copyable& = default;
};

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
export class non_moveable {
	protected:
		non_moveable() = default;

	public:
		non_moveable(const non_moveable&) = delete;
		auto operator=(const non_moveable&) = delete;
};

// Return a sequence of index constants
export template <std::size_t Size>
constexpr auto sequence = []() consteval -> auto {
	// With C++26 P2686 we can do constexpr decomposition. So instead of the `tuple` of
	// `integral_constants` it can be an array of `size_t`.
	// https://clang.llvm.org/cxx_status.html

	// std::array<std::size_t, Size> result{};
	// std::ranges::copy(std::ranges::iota_view{std::size_t{0}, Size}, result.begin());
	// return result;

	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{std::integral_constant<std::size_t, Index>{}...};
	}(std::make_index_sequence<Size>());
}();

// Allocates storage for a copy of a constant expression value and initializes it with the value.
export template <const auto& Value, class Type = std::remove_cvref_t<decltype(Value)>>
struct copy_of : Type {
		constexpr copy_of() :
				Type(Value) {}
};

// Wraps the given invocable. When invoked if it returns `void` then `std::monostate{}` will be
// returned instead.
export template <class Invoke>
class regular_return {
	public:
		constexpr explicit regular_return(Invoke invoke) :
				invoke{std::move(invoke)} {}

		constexpr auto operator()(auto&&... args) -> decltype(auto)
			requires std::invocable<Invoke&, decltype(args)...>
		{
			if constexpr (std::invoke_result<Invoke&, decltype(args)...>{} == type<void>) {
				invoke(std::forward<decltype(args)>(args)...);
				return std::monostate{};
			} else {
				return invoke(std::forward<decltype(args)>(args)...);
			}
		}

	private:
		Invoke invoke;
};

// https://en.cppreference.com/w/cpp/experimental/scope_exit
export template <class Invoke>
class scope_exit : non_copyable {
	public:
		explicit scope_exit(Invoke invoke) :
				invoke_{std::move(invoke)} {}
		scope_exit(const scope_exit&) = delete;
		~scope_exit() { invoke_(); }
		auto operator=(const scope_exit&) -> scope_exit& = delete;

	private:
		Invoke invoke_;
};

// Explicitly annotate desired struct slicing: cppcoreguidelines-slicing
export template <class Type>
auto slice_cast(const std::derived_from<Type> auto& value) -> const Type& {
	return static_cast<const Type&>(value);
}

} // namespace util
