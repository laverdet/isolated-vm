#include <napi.h>
#include <memory>
import ivm.node;
import ivm.utility;
import ivm.value;
import napi;

template <class Function>
class node_function;

template <class Result, class... Args>
class node_function<Result (*)(Napi::Env, ivm::environment&, Args...)> : non_copyable {
	public:
		using function_type = Result (*)(Napi::Env, ivm::environment&, Args...);

		node_function() = delete;
		explicit node_function(ivm::environment& ienv, function_type fn) :
				ienv{&ienv},
				fn{std::move(fn)} {}

		auto operator()(const Napi::CallbackInfo& info) -> Napi::Value {
			ivm::napi::napi_callback_info_memo callback_info{info, *ienv};
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(info.Env(), *ienv),
					ivm::value::transfer<std::tuple<Args...>>(callback_info)
				)
			);
		}

	private:
		ivm::environment* ienv;
		function_type fn;
};

auto auto_wrap(Napi::Env env, ivm::environment& ienv, auto fn) {
	return Napi::Function::New(env, node_function<decltype(fn)>{ienv, fn});
}

auto isolated_vm(Napi::Env env, Napi::Object exports) -> Napi::Object {
	// Initialize isolated-vm environment for this nodejs context
	auto ienv = std::make_unique<ivm::environment>();

	exports.Set(Napi::String::New(env, "compileScript"), auto_wrap(env, *ienv, ivm::compile_script));
	exports.Set(Napi::String::New(env, "createAgent"), auto_wrap(env, *ienv, ivm::create_agent));
	exports.Set(Napi::String::New(env, "createRealm"), auto_wrap(env, *ienv, ivm::create_realm));
	exports.Set(Napi::String::New(env, "runScript"), auto_wrap(env, *ienv, ivm::run_script));

	env.SetInstanceData(ienv.release());
	return exports;
}

NODE_API_MODULE(isolated_vm, isolated_vm);
