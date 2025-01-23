module;
#include <type_traits>
export module napi_js.value.internal;
import nodejs;

namespace js::napi {

// Base class of napi values
export class indirect_value {
	protected:
		explicit indirect_value(napi_value value) :
				value_{value} {}

	public:
		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const { return value_; }

		auto operator==(const indirect_value& right) const -> bool { return value_ == right.value_; }

	private:
		napi_value value_;
};

// Method implementation for each tagged type
export template <class Tag>
struct implementation : implementation<typename Tag::tag_type> {};

template <>
struct implementation<void> : indirect_value {};

} // namespace js::napi
