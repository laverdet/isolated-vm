module;
#include <type_traits>
#include <utility>
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
export template <auto Value, class Type = decltype(Value)>
struct copy_of;

template <auto Value, class Type>
struct copy_of<Value, const Type*> : Type {
		constexpr copy_of() :
				Type{*Value} {}
};

// Convert static function pointer into a default-constructible function type
export template <auto Function>
struct function_type_of;

template <class Result, class... Args, Result(Function)(Args...)>
struct function_type_of<Function> {
		auto operator()(Args... args) const -> decltype(auto) {
			return Function(std::forward<Args>(args)...);
		}
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

// Wrapper which does not initialize the data member. For use in `std::vector`.
export template <class Type>
	requires std::is_trivially_destructible_v<Type>
struct trivial_aggregate {
	private:
		// NOLINTNEXTLINE(modernize-avoid-c-arrays)
		std::byte data[ sizeof(Type) ];
};

} // namespace util
