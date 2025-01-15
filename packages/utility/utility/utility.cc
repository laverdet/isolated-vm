module;
#include <utility>
export module ivm.utility.utility;

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

// Convert static function pointer into a default-constructible function type
export template <auto Function>
struct function_type_of {
		auto operator()(auto&&... args) const -> decltype(auto)
			requires std::invocable<decltype(Function), decltype(args)...> {
			return Function(std::forward<decltype(args)>(args)...);
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

} // namespace util
