export module napi_js:runtime;
import :handle.local_of;
import :reference;
import nodejs;
import util;

namespace js::napi {

// Runtime support functions from `runtime/runtime.ts`
class runtime : util::non_moveable {
	public:
		explicit runtime(napi_env env);
		auto async_to_callbacks(napi_env env) const -> local_of<function_tag>;

	private:
		reference<function_tag> async_to_callbacks_;
};

} // namespace js::napi
