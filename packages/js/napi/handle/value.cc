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
class value : public value_handle {
	public:
		using value_handle::value_handle;

		// "Downcast" from a more specific tag
		template <std::convertible_to<Tag> From>
		// NOLINTNEXTLINE(google-explicit-constructor)
		value(value<From> value_) :
				value_handle{napi_value{value_}} {}

		// "Upcast" to a more specific tag. Potentially unsafe.
		template <std::convertible_to<Tag> To>
		auto cast(To /*tag*/) const -> value<To> { return value<To>::from(*this); }

		// Access underlying implementation. `operator*()` intentionally not implemented.
		auto operator->() -> implementation<Tag>* {
			static_assert(std::is_layout_compatible_v<value, implementation<Tag>>);
			value_handle* value = this;
			return static_cast<implementation<Tag>*>(value);
		}

		// Construct from any `napi_value`. Potentially unsafe.
		static auto from(napi_value value_) -> value { return value{value_}; }

		// Make a new `napi_value` with this value tag
		static auto make(auto_environment auto& env, auto&&... args) -> value {
			return implementation<Tag>::make(env, std::forward<decltype(args)>(args)...);
		}
};

// Member & method implementation for stateful objects. Used internally in visitors.
export template <class Tag>
class bound_value : public bound_value<typename Tag::tag_type> {
	public:
		bound_value(napi_env env, value<Tag> value) :
				bound_value<typename Tag::tag_type>{env, value} {}
};

template <>
class bound_value<void> : public value_handle {
	protected:
		explicit bound_value(napi_env env, napi_value value) :
				value_handle{value},
				env_{env} {}

		[[nodiscard]] auto env() const { return env_; }

	private:
		napi_env env_;
};

} // namespace js::napi
