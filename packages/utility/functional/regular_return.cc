module;
#include <utility>
export module ivm.utility:functional.regular_return;
import :utility;
import :type_traits;

namespace util {

// Value wrapper which can accept void values. `operator*` will return the underlying value.
template <class Type>
class regular_value {
	public:
		constexpr explicit regular_value(Type value) :
				value_{std::move(value)} {}

		constexpr auto operator*() && -> Type { return std::move(value_); }
		constexpr auto value_or(const auto& /*nothing*/) && -> Type { return *std::move(*this); };

	private:
		Type value_;
};

template <>
class regular_value<void> {
	public:
		constexpr auto operator*() const -> void {}

		template <class Type>
		constexpr auto value_or(Type value) -> Type { return value; };
};

// Wraps the given invocable. When invoked it returns `regular_value<T>` of the function return
// value.
export template <class Type>
class regular_return {
	public:
		constexpr explicit regular_return(Type invoke) :
				invoke_{std::move(invoke)} {}

		constexpr auto operator()(this auto&& self, auto&&... args)
			-> regular_value<std::invoke_result_t<util::apply_cvref_t<decltype(self), Type>, decltype(args)...>> {
			using result_type = std::invoke_result_t<util::apply_cvref_t<decltype(self), Type>, decltype(args)...>;
			if constexpr (std::is_void_v<result_type>) {
				util::forward_from<decltype(self)>(self.invoke_)(std::forward<decltype(args)>(args)...);
				return regular_value<void>{};
			} else {
				return regular_value<result_type>{util::forward_from<decltype(self)>(self.invoke_)(std::forward<decltype(args)>(args)...)};
			}
		}

	private:
		Type invoke_;
};

} // namespace util
