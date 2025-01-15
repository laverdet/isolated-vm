module;
#include <expected>
export module ivm.node:environment;
import ivm.isolated_v8;
import ivm.js;
import ivm.napi;
import ivm.utility;
import nodejs;
import v8;

namespace ivm {

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

// Module forward declarations
export auto make_compile_module(environment& env) -> js::napi::value<js::function_tag>;
export auto make_compile_script(environment& env) -> js::napi::value<js::function_tag>;
export auto make_create_agent(environment& env) -> js::napi::value<js::function_tag>;
export auto make_create_realm(environment& env) -> js::napi::value<js::function_tag>;
export auto make_evaluate_module(environment& env) -> js::napi::value<js::function_tag>;
export auto make_link_module(environment& env) -> js::napi::value<js::function_tag>;
export auto make_run_script(environment& env) -> js::napi::value<js::function_tag>;

} // namespace ivm
