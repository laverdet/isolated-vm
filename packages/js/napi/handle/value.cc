module;
#include <concepts>
export module napi_js.value;
import nodejs;
import isolated_js;

namespace js::napi {

// Forward declaration
export template <class Tag> class value;

namespace detail {

// `detail:value_handle` is the base class of `value<T>`, and `bound_value<T>`.
export class value_handle {
	public:
		value_handle() = default;
		explicit value_handle(napi_value value) :
				value_{value} {}

		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const { return value_; }

		// Check for empty value
		explicit operator bool() const { return value_ != nullptr; }

	private:
		napi_value value_{};
};

// Details applied to each level of the `value<T>` hierarchy.
export template <class Tag>
class value_next : public value<typename Tag::tag_type> {
	public:
		using value<typename Tag::tag_type>::value;

		// "Upcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> { return value<To>::from(*this); }

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value<Tag> { return value<Tag>{value_}; }
};

} // namespace detail

// Tagged napi_value
export template <class Tag>
class value : public detail::value_next<Tag> {
	public:
		using detail::value_next<Tag>::value_next;
};

template <>
class value<void> : public detail::value_handle {
		using value_handle::value_handle;
};

} // namespace js::napi
