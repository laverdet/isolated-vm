#include <napi.h>
#include <memory>
import ivm.node;
import ivm.value;
import napi;

template <class Function>
class node_function;

template <class Result, class... Args>
class node_function<Result (*)(Napi::Env, Args...)> {
	public:
		using function_type = Result (*)(Napi::Env, Args...);

		explicit node_function(function_type fn) :
				fn{std::move(fn)} {}

		auto operator()(const Napi::CallbackInfo& info) -> Napi::Value {
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(info.Env()),
					ivm::value::transfer<std::tuple<Args...>>(info)
				)
			);
		}

	private:
		function_type fn;
};

auto auto_wrap(Napi::Env env, auto fn) {
	return Napi::Function::New(env, node_function<decltype(fn)>{fn});
}

auto isolated_vm(Napi::Env env, Napi::Object exports) -> Napi::Object {
	// Initialize isolated-vm environment for this nodejs context
	auto environment = std::make_unique<ivm::environment>();
	// TODO: cleanup is sync, it could be async
	env.SetInstanceData(environment.release());

	exports.Set(Napi::String::New(env, "compileScript"), auto_wrap(env, ivm::compile_script));
	exports.Set(Napi::String::New(env, "createAgent"), auto_wrap(env, ivm::create_agent));
	exports.Set(Napi::String::New(env, "createRealm"), auto_wrap(env, ivm::create_realm));
	exports.Set(Napi::String::New(env, "runScript"), auto_wrap(env, ivm::run_script));

	return exports;
}

NODE_API_MODULE(isolated_vm, isolated_vm);
