module;
#include <concepts>
#include <type_traits>
#include <utility>
export module napi_js.value;
import isolated_js;
import napi_js.environment;
import napi_js.value.internal;
import nodejs;

namespace js::napi {

// Simple wrapper around `napi_value` denoting underlying value type
export template <class Tag>
class value : public indirect_value {
	public:
		using indirect_value::indirect_value;

		// "Downcast" from a more specific tag
		template <std::convertible_to<Tag> From>
		// NOLINTNEXTLINE(google-explicit-constructor)
		value(value<From> value_) :
				indirect_value{napi_value{value_}} {}

		// "Upcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> { return value<To>::from(*this); }

		// Access underlying implementation. `operator*()` intentionally not implemented.
		auto operator->() -> implementation<Tag>* {
			static_assert(std::is_layout_compatible_v<value, implementation<Tag>>);
			indirect_value* indirect = this;
			return static_cast<implementation<Tag>*>(indirect);
		}

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value { return value{value_}; }

		// Make a new `napi_value` with this value tag
		static auto make(auto_environment auto& env, auto&&... args) -> value {
			return implementation<Tag>::make(env, std::forward<decltype(args)>(args)...);
		}
};

} // namespace js::napi
