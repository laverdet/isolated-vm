module;
#include <utility>
export module isolated_js:deferred_receiver;

namespace js {

struct default_dispatch {
		constexpr auto operator()(const auto& /*receiver*/) const -> void {}
};

// Wrapper for an accepted "receiver" type. The value is created first with no properties so that
// references can be setup correctly.
export template <class Type, class... Args>
class deferred_receiver {
	private:
		using dispatch_type = auto(Type, Args...) -> void;

	public:
		constexpr deferred_receiver(Type value, dispatch_type* dispatch) :
				value_{std::move(value)},
				dispatch_{dispatch} {}

		auto operator*() const -> const Type& { return value_; }
		auto operator()(Args... args) && -> void { dispatch_(value_, std::forward<Args>(args)...); }

	private:
		Type value_;
		dispatch_type* dispatch_;
};

} // namespace js
