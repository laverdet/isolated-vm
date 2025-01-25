module;
#include <expected>
export module backend_napi_v8.environment;
import isolated_v8;
import isolated_js;
import napi_js;
import ivm.utility;
import nodejs;
import v8;

namespace backend_napi_v8 {

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment
		: util::non_moveable,
			public js::napi::environment_of<environment>,
			public js::napi::uv_schedulable {
	public:
		explicit environment(napi_env env);
		~environment();

		auto cluster() -> isolated_v8::cluster& { return cluster_; }
		static auto get(napi_env env) -> environment&;

	private:
		isolated_v8::cluster cluster_;
		v8::Isolate* isolate_;
};

} // namespace backend_napi_v8
