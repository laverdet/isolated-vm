module;
#include <concepts>
#include <cstddef>
#include <utility>
#include <variant>
export module napi_js.value;
import isolated_js;
import nodejs;

namespace js::napi {

// napi_value factory
export template <class Tag>
struct factory;

export template <class Tag>
class value;

// Base specialization which provides a constructor & `null` / `undefined` value constructors
template <>
struct factory<value_tag> {
	public:
		explicit factory(napi_env env) :
				env_{env} {}

		[[nodiscard]] auto env() const -> napi_env { return env_; }
		auto operator()(std::monostate /*undefined*/) const -> value<undefined_tag>;
		auto operator()(std::nullptr_t /*null*/) const -> value<null_tag>;

	private:
		napi_env env_;
};

// undefined & null value factories
template <>
struct factory<undefined_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		using factory<value_tag>::operator();
		auto operator()() const -> value<undefined_tag>;
};

template <>
struct factory<null_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		using factory<value_tag>::operator();
		auto operator()() const -> value<null_tag>;
};

// Simple wrapper around `napi_value` denoting underlying value type
template <class Tag>
class value {
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
		static auto make(napi_env env, auto&&... args) -> value {
			return factory<Tag>{env}(std::forward<decltype(args)>(args)...);
		}

	private:
		napi_value value_;
};

} // namespace js::napi
