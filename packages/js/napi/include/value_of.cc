module;
#include <concepts>
export module ivm.napi:value_of;
import napi;

namespace ivm::js::napi {

// Simple wrapper around `napi_value` denoting underlying value type
export template <class Tag>
class value_of {
	private:
		explicit value_of(napi_value value) :
				value_{value} {}

	public:
		// "Downcast" from a more specific tag
		template <std::convertible_to<Tag> From>
		// NOLINTNEXTLINE(google-explicit-constructor)
		value_of(value_of<From> value) :
				value_of{napi_value{value}} {}

		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const {
			return value_;
		}

		// "Upcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value_of<To> {
			return value_of<To>::from(value_);
		}

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value) -> value_of {
			return value_of{value};
		}

	private:
		napi_value value_;
};

} // namespace ivm::js::napi
