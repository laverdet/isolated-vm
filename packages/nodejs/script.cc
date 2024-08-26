module;
#include <memory>
#include <utility>
module ivm.node;
import :environment;
import :utility;
import :visit;
import ivm.isolated_v8;
import ivm.v8;
import ivm.value;
import napi;

namespace ivm {

auto compile_script(
	Napi::Env env,
	environment& ienv,
	iv8::collected_external<agent>& agent,
	ivm::value::string_t code_string
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<ivm::script>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::script> script) -> expected_value {
			auto script_handle = iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*script));
			return value::direct_cast<Napi::Value>(script_handle, env, ienv);
		}
	);
	agent->schedule_task(
		[ code_string = std::move(code_string),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& lock
		) mutable {
			dispatch(ivm::script::compile(lock, std::move(code_string)));
		}
	);
	return promise;
}

auto run_script(
	Napi::Env env,
	environment& ienv,
	iv8::collected_external<agent>& agent,
	iv8::collected_external<script>& script,
	iv8::collected_external<realm>& realm
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<value::value_t>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<value::value_t> result) -> expected_value {
			return value::transfer_strict<Napi::Value>(std::move(*result), env, ienv);
		}
	);
	agent->schedule_task(
		[ &realm,
			&script,
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& lock
		) mutable {
			ivm::realm::scope realm_scope{lock, *realm};
			auto result = script->run(realm_scope);
			dispatch(std::move(result));
		}
	);
	return promise;
}

auto make_compile_script(Napi::Env env, ivm::environment& ienv) -> Napi::Function {
	return make_node_function(env, ienv, ivm::compile_script);
}

auto make_run_script(Napi::Env env, ivm::environment& ienv) -> Napi::Function {
	return make_node_function(env, ienv, ivm::run_script);
}

} // namespace ivm
