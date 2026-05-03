export module util:utility;
export import :utility.constant_wrapper;
export import :utility.covariant_value;
export import :utility.facade;
export import :utility.hash;
export import :utility.ranges;
export import :utility.string;
// export import :utility.variant;
import std;

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
	// `integral_constants` it can be an array of `std::size_t`.
	// https://clang.llvm.org/cxx_status.html

	// std::array<std::size_t, Size> result{};
	// std::ranges::copy(std::ranges::views::iota(std::size_t{0}, Size), result.begin());
	// return result;

	return []<std::size_t... Index>(std::index_sequence<Index...> /*sequence*/) consteval {
		return std::tuple{std::integral_constant<std::size_t, Index>{}...};
	}(std::make_index_sequence<Size>());
}();

// Checked container / range access for types which don't have `.at()`
export auto at(auto&& range, std::size_t index) -> decltype(auto) {
	if (range.size() <= index) {
		throw std::out_of_range{"Index out of range"};
	}
	// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-constant-array-index)
	return std::forward<decltype(range)>(range)[ index ];
}

// Allocates storage for a copy of a constant expression value and initializes it with the value.
export template <const auto& Value, class Type = std::remove_cvref_t<decltype(Value)>>
struct copy_of : Type {
		constexpr copy_of() :
				Type(Value) {}
};

// Remove an element from a (probably) `std::vector` by swap and popping
export auto swap_and_pop(auto& container, auto iterator) -> void {
	if (std::next(iterator) != container.end()) {
		std::swap(*iterator, container.back());
	}
	container.pop_back();
}

// Deref'able holder which always contains a value
export template <class Type>
class just : public pointer_facade {
	public:
		explicit constexpr just(const Type& value) : value_{value} {}
		explicit constexpr just(Type&& value) : value_{std::move(value)} {}
		explicit constexpr operator bool() const { return true; }
		constexpr auto operator*(this auto&& self) { return std::forward<decltype(self)>(self).value_; }

	private:
		Type value_;
};

template <class Type>
class just<Type&> : public pointer_facade {
	public:
		explicit constexpr just(Type& value) : value_{&value} {}
		explicit constexpr operator bool() const { return true; }
		constexpr auto operator*() const -> Type& { return *value_; }

	private:
		Type* value_;
};

// Like `just<T>` but never contains a value
export template <class Type>
class nothing : public pointer_facade {
	public:
		explicit constexpr operator bool() const { return false; }
		constexpr auto operator*() const -> Type { std::unreachable(); }
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
template <class Type>
struct slice_t;

template <class Type>
struct slice_t<const Type> {
	public:
		explicit slice_t(const Type& value) : value_{value} {}

		template <class As>
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator const As&() &&
			requires(std::convertible_to<Type&, As&> && type<Type> != type<As>) {
			return static_cast<const As&>(value_.get());
		}

	private:
		std::reference_wrapper<const Type> value_;
};

export template <class Type>
constexpr auto slice(const Type& value) {
	return slice_t<const Type>{value};
}

} // namespace util
