module;
#include <utility>
export module ivm.napi:reference;
import :utility;
import napi;

namespace ivm::js::napi {

export class reference : util::non_copyable {
	public:
		reference(napi_env env, napi_value value) :
				env_{env},
				value_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		reference(reference&& right) noexcept :
				env_{right.env_},
				value_{std::exchange(right.value_, nullptr)} {}

		~reference() {
			if (value_ != nullptr) {
				js::napi::invoke_dtor(napi_delete_reference, env_, value_);
			}
		}

		auto operator=(reference&& right) = delete;

		auto operator*() const -> napi_value {
			return js::napi::invoke(napi_get_reference_value, env_, value_);
		}

	private:
		napi_env env_;
		napi_ref value_;
};

} // namespace ivm::js::napi
