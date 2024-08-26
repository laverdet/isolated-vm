#include <napi.h>
import ivm.node;
import napi;

auto isolated_vm(Napi::Env env, Napi::Object exports) -> Napi::Object {
	// Initialize isolated-vm environment for this nodejs context
	auto ienv = std::make_unique<ivm::environment>();

	exports.Set(Napi::String::New(env, "compileScript"), ivm::make_compile_script(env, *ienv));
	exports.Set(Napi::String::New(env, "createAgent"), ivm::make_create_agent(env, *ienv));
	exports.Set(Napi::String::New(env, "createRealm"), ivm::make_create_realm(env, *ienv));
	exports.Set(Napi::String::New(env, "runScript"), ivm::make_run_script(env, *ienv));

	env.SetInstanceData(ienv.release());
	return exports;
}

NODE_API_MODULE(isolated_vm, isolated_vm);
