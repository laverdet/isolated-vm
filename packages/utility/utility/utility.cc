module;
#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.utility:utility;

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
			if constexpr (std::is_void_v<std::invoke_result_t<Invoke, decltype(args)...>>) {
				invoke();
				return std::monostate{};
			} else {
				return invoke();
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
