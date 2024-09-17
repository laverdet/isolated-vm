module;
#include <utility>
module ivm.node;
import :environment;
import :external;
import :utility;
import :visit;
import ivm.isolated_v8;
import ivm.v8;
import ivm.value;
import napi;

namespace ivm {

auto compile_script(
	environment& env,
	iv8::external_reference<agent>& agent,
	value::string_t code_string
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<ivm::script>(env, [](environment& env, ivm::script script) -> expected_value {
		return make_collected_external<ivm::script>(env, std::move(script));
	});
	agent->schedule(
		[ code_string = std::move(code_string),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			dispatch(ivm::script::compile(agent, std::move(code_string)));
		}
	);
	return promise;
}

auto run_script(
	environment& env,
	iv8::external_reference<agent>& agent,
	iv8::external_reference<script>& script,
	iv8::external_reference<realm>& realm
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<value::value_t>(env, [](environment& env, value::value_t result) -> expected_value {
		return value::transfer_strict<Napi::Value>(std::move(result), env.napi_env());
	});
	agent->schedule(
		[ &realm,
			&script,
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			ivm::realm::managed_scope realm_scope{agent, *realm};
			auto result = script->run(realm_scope);
			dispatch(std::move(result));
		}
	);
	return promise;
}

auto make_compile_script(environment& env) -> Napi::Function {
	return make_node_function(env, compile_script);
}

auto make_run_script(environment& env) -> Napi::Function {
	return make_node_function(env, run_script);
}

} // namespace ivm
