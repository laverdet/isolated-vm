module;
#include <expected>
export module backend_napi_v8.environment;
import isolated_v8;
import isolated_js;
import ivm.napi;
import ivm.utility;
import nodejs;
import v8;

namespace backend_napi_v8 {

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment : util::non_moveable {
	public:
		explicit environment(napi_env env);
		~environment();

		auto cluster() -> isolated_v8::cluster& { return cluster_; }
		auto nenv() -> napi_env { return env_; }
		auto isolate() -> v8::Isolate* { return isolate_; }
		auto scheduler() -> js::napi::uv_scheduler& { return scheduler_; };
		static auto get(napi_env env) -> environment&;

	private:
		napi_env env_;
		js::napi::uv_scheduler scheduler_;
		isolated_v8::cluster cluster_;
		v8::Isolate* isolate_;
};

} // namespace backend_napi_v8
