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
		environment();

		auto cluster() -> ivm::cluster&;
		auto collection() -> collection_group&;
		auto context() -> v8::Local<v8::Context>;
		auto isolate() -> v8::Isolate*;

		static auto get(Napi::Env env) -> environment&;

	private:
		ivm::cluster cluster_;
		v8::Isolate* isolate_;
		v8::Persistent<v8::Context> context_;
		collection_group collection_group_;
};

// Module forward declarations
export auto make_compile_script(Napi::Env env, ivm::environment& ienv) -> Napi::Function;
export auto make_create_agent(Napi::Env env, ivm::environment& ienv) -> Napi::Function;
export auto make_create_realm(Napi::Env env, ivm::environment& ienv) -> Napi::Function;
export auto make_run_script(Napi::Env env, ivm::environment& ienv) -> Napi::Function;

} // namespace ivm
