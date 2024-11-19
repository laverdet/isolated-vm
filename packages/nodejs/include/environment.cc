module;
#include <expected>
export module ivm.node:environment;
import ivm.isolated_v8;
import ivm.utility;
import napi;
import v8;

namespace ivm {

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment {
	public:
		explicit environment(napi_env env);

		auto cluster() -> ivm::cluster&;
		auto collection() -> util::collection_group&;
		auto nenv() -> napi_env;
		auto isolate() -> v8::Isolate*;
		static auto get(napi_env env) -> environment&;

	private:
		napi_env env_;
		ivm::cluster cluster_;
		v8::Isolate* isolate_;
		util::collection_group collection_group_;
};

// Module forward declarations
export auto make_compile_module(environment& env) -> napi_value;
export auto make_compile_script(environment& env) -> napi_value;
export auto make_create_agent(environment& env) -> napi_value;
export auto make_create_realm(environment& env) -> napi_value;
export auto make_run_script(environment& env) -> napi_value;

} // namespace ivm
