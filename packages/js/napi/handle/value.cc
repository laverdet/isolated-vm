module;
#include <concepts>
export module napi_js.value;
import nodejs;
import isolated_js;

namespace js::napi {

// Forward declarations
export template <class Tag> class value;
export template <class Tag> class bound_value;

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

// Details applied to each level of the `bound_value<T>` hierarchy.
export template <class Tag>
class bound_value_next : public bound_value<typename Tag::tag_type> {
	public:
		using bound_value<typename Tag::tag_type>::bound_value;
		bound_value_next() = default;
		bound_value_next(napi_env env, value<Tag> value) :
				bound_value<typename Tag::tag_type>{env, napi_value{value}} {}

		explicit operator value<Tag>() const { return value<Tag>::from(napi_value{*this}); }
};

// Member & method implementation for stateful objects. Used internally in visitors. I think it
// might make sense to have the environment specified by a template parameter. Then you would use
// `bound_value<T>` or something instead of passing the environment to each `value<T>` method.
export template <class Tag>
class bound_value : public bound_value_next<Tag> {
	public:
		using bound_value_next<Tag>::bound_value_next;
};

template <class Tag>
bound_value(auto, value<Tag>) -> bound_value<Tag>;

template <>
class bound_value<void> : public detail::value_handle {
	protected:
		bound_value() = default;
		bound_value(napi_env env, napi_value value) :
				value_handle{value},
				env_{env} {}

		[[nodiscard]] auto env() const { return env_; }

	private:
		napi_env env_{};
};

} // namespace js::napi
