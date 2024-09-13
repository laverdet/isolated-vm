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
		explicit environment(Napi::Env env);

		auto cluster() -> ivm::cluster&;
		auto collection() -> collection_group&;
		auto context() -> v8::Local<v8::Context>;
		auto napi_env() -> Napi::Env;
		auto isolate() -> v8::Isolate*;
		auto with_context_local() -> scope_exit<defaulter_finalizer<v8::Local<v8::Context>>>;

		static auto get(Napi::Env env) -> environment&;

	private:
		Napi::Env env_;
		ivm::cluster cluster_;
		v8::Isolate* isolate_;
		v8::Local<v8::Context> context_;
		collection_group collection_group_;
};

// Module forward declarations
export auto make_compile_script(environment& env) -> Napi::Function;
export auto make_create_agent(environment& env) -> Napi::Function;
export auto make_create_realm(environment& env) -> Napi::Function;
export auto make_run_script(environment& env) -> Napi::Function;

} // namespace ivm
