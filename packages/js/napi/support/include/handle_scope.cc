export module ivm.napi:handle_scope;
import :utility;
import isolated_js;
import ivm.utility;
import nodejs;

namespace js::napi {

export class handle_scope : util::non_moveable {
	public:
		explicit handle_scope(napi_env env) :
				env_{env},
				handle_scope_{js::napi::invoke(napi_open_handle_scope, env)} {}

		~handle_scope() {
			js::napi::invoke_dtor(napi_close_handle_scope, env_, handle_scope_);
		}

	private:
		napi_env env_;
		napi_handle_scope handle_scope_;
};

} // namespace js::napi
