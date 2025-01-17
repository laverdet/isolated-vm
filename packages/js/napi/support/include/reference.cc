module;
#include <utility>
export module napi_js.reference;
import ivm.utility;
import napi_js.utility;
import napi_js.value;
import nodejs;

namespace js::napi {

export template <class Tag>
class reference : util::non_copyable {
	public:
		reference(napi_env env, value<Tag> value) :
				env_{env},
				value_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		reference(const reference&) = delete;
		reference(reference&& right) noexcept :
				env_{right.env_},
				value_{std::exchange(right.value_, nullptr)} {}

		~reference() {
			if (value_ != nullptr) {
				js::napi::invoke_dtor(napi_delete_reference, env_, value_);
			}
		}

		auto operator=(const reference&) = delete;
		auto operator=(reference&&) = delete;

		auto operator*() const -> value<Tag> {
			return value<Tag>::from(js::napi::invoke(napi_get_reference_value, env_, value_));
		}

	private:
		napi_env env_;
		napi_ref value_;
};

} // namespace js::napi
