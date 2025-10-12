module;
#include <concepts>
#include <utility>
export module isolated_js:deferred_receiver;
import ivm.utility;

namespace js {

struct default_dispatch {
		constexpr auto operator()(const auto& /*receiver*/) const -> void {}
};

// Wrapper for a value which can be referenced, for efficiency (like string), or for correctness
// (date) but is not a receiver type.
export template <class Type>
class referenceable_value {
	public:
		explicit constexpr referenceable_value(Type value) :
				value_{std::move(value)} {}

		template <std::convertible_to<Type> From>
		explicit constexpr referenceable_value(referenceable_value<From> value) :
				value_{*std::move(value)} {}

		constexpr auto operator*() const& -> const Type& { return value_; }
		constexpr auto operator*() && -> Type { return std::move(value_); }

	private:
		Type value_;
};

// Wrapper for an accepted "receiver" type. The value is created first with no properties so that
// references can be setup correctly.
export template <class Type, class... Args>
class deferred_receiver : public referenceable_value<Type> {
	private:
		using dispatch_type = auto(Type, Args...) -> void;

	public:
		constexpr deferred_receiver(Type value, std::tuple<Args...> args, dispatch_type* dispatch) :
				referenceable_value<Type>{std::move(value)},
				dispatch_{dispatch},
				args_{std::move(args)} {}

		constexpr auto operator()() && -> void {
			const auto [... indices ] = util::sequence<sizeof...(Args)>;
			dispatch_(*std::move(*this), std::get<indices>(std::move(args_))...);
		}

	private:
		dispatch_type* dispatch_;
		std::tuple<Args...> args_;
};

} // namespace js
