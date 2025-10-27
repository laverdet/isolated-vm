module;
#include <concepts>
export module napi_js:value_handle;
import nodejs;

namespace js::napi {

// Forward declaration
export template <class Tag> class value;

// `value_handle` is the base class of `value<T>`, and `bound_value<T>`.
class value_handle {
	protected:
		explicit value_handle(napi_value value) :
				value_{value} {}

	public:
		value_handle() = default;

		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const { return value_; }

		// Check for empty value
		explicit operator bool() const { return value_ != nullptr; }

	private:
		napi_value value_{};
};

// Details applied to each level of the `value<T>` hierarchy.
template <class Tag>
class value_next : public value<typename Tag::tag_type> {
	public:
		using value<typename Tag::tag_type>::value;

		// "Downcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> { return value<To>::from(*this); }

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value<Tag> { return value<Tag>{value_}; }
};

// Tagged napi_value
template <class Tag>
class value : public value_next<Tag> {
	public:
		using value_next<Tag>::value_next;
};

template <>
class value<void> : public value_handle {
		using value_handle::value_handle;
};

} // namespace js::napi
