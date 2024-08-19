module;
#include <concepts>
#include <type_traits>
#include <utility>
export module ivm.utility:only_cast;
import :type_traits;

// `only_cast<T>` selects the given cast operator on an underlying value. The resulting object has
// only one method which is a cast operation.
export template <class To, class Type>
	requires std::constructible_from<To, Type> and std::move_constructible<Type>
struct only_cast {
	public:
		explicit only_cast(same_as_cvref<Type> auto&& value) :
				value{std::forward<decltype(value)>(value)} {}
		operator To() { return static_cast<To>(value); }						 // NOLINT(google-explicit-constructor)
		operator To() const { return static_cast<const To>(value); } // NOLINT(google-explicit-constructor)

	private:
		Type value;
};

export template <class Type>
auto make_only_cast(auto&& value) {
	return only_cast<Type, std::remove_reference_t<decltype(value)>>{std::forward<decltype(value)>(value)};
}
