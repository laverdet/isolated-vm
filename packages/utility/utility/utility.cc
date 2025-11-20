module;
#include <concepts>
#include <utility>
export module util:utility;
export import :utility.constant_wrapper;
export import :utility.covariant_value;
export import :utility.facade;
export import :utility.hash;
export import :utility.ranges;
// export import :utility.variant;
import :type_traits;

namespace util {

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
struct slice {
	public:
		explicit slice(Type& value) : value_{value} {}

		template <class As>
			requires std::derived_from<std::remove_cv_t<Type>, As>
		// NOLINTNEXTLINE(google-explicit-constructor)
		constexpr operator As() const {
			return As{value_};
		}

	private:
		std::reference_wrapper<Type> value_;
};

} // namespace util
