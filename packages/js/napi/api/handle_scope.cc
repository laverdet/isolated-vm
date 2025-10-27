export module napi_js:api.handle_scope;
import :api.invoke;
import ivm.utility;

namespace js::napi {

// RAII napi handle scope
export class handle_scope : util::non_moveable {
	public:
		explicit handle_scope(napi_env env) :
				env_{env},
				handle_scope_{napi::invoke(napi_open_handle_scope, env)} {}

		~handle_scope() {
			napi::invoke0_noexcept(napi_close_handle_scope, env_, handle_scope_);
		}

	private:
		napi_env env_;
		napi_handle_scope handle_scope_;
};

} // namespace js::napi
