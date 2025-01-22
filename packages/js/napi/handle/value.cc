module;
#include <concepts>
#include <utility>
export module napi_js.value;
import isolated_js;
import napi_js.environment;
import nodejs;

namespace js::napi {

export template <class Tag>
struct implementation {};

// Simple wrapper around `napi_value` denoting underlying value type
export template <class Tag>
class value : public implementation<Tag> {
	private:
		explicit value(napi_value value) :
				value_{value} {}

	public:
		// "Downcast" from a more specific tag
		template <std::convertible_to<Tag> From>
		// NOLINTNEXTLINE(google-explicit-constructor)
		value(value<From> value_) :
				value{napi_value{value_}} {}

		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const {
			return value_;
		}

		// "Upcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> {
			return value<To>::from(value_);
		}

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value {
			return value{value_};
		}

		// Make a new `napi_value` with this value tag
		static auto make(auto_environment auto& env, auto&&... args) -> value {
			return implementation<Tag>::make(env, std::forward<decltype(args)>(args)...);
		}

	private:
		napi_value value_;
};

} // namespace js::napi
