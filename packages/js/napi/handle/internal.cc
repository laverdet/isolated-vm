module;
#include <cassert>
#include <utility>
export module napi_js.value.internal;
import ivm.utility;
import napi_js.utility;
import nodejs;

namespace js::napi {

// Base class of `value<T>`
export class value_handle {
	protected:
		explicit value_handle(napi_value value) :
				value_{value} {}

	public:
		// Implicit cast back to a `napi_value`
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator napi_value() const { return value_; }

		auto operator==(const value_handle& right) const -> bool { return value_ == right.value_; }

	private:
		napi_value value_;
};

// Method implementation for each tagged type
export template <class Tag>
struct implementation : implementation<typename Tag::tag_type> {};

template <>
struct implementation<void> : value_handle {};

// Base class of reference & remote values
export class reference_handle : util::non_copyable {
	protected:
		reference_handle(napi_env env, napi_value value) :
				env_{env},
				value_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		[[nodiscard]] auto env() const -> napi_env { return env_; }

		[[nodiscard]] auto get(napi_env env) const -> napi_value {
			return js::napi::invoke(napi_get_reference_value, env, value_);
		}

	public:
		reference_handle(const reference_handle&) = delete;
		reference_handle(reference_handle&& right) noexcept :
				env_{right.env_},
				value_{std::exchange(right.value_, nullptr)} {}

		~reference_handle() {
			if (value_ != nullptr) {
				js::napi::invoke_dtor(napi_delete_reference, env_, value_);
			}
		}

		auto operator=(const reference_handle&) = delete;
		auto operator=(reference_handle&&) = delete;

	private:
		napi_env env_;
		napi_ref value_;
};

} // namespace js::napi
