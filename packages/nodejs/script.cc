module;
#include <memory>
#include <utility>
export module ivm.node:script;
import :environment;
import :make_promise;
import :visit;
import ivm.isolated_v8;
import ivm.v8;
import ivm.value;
import napi;

namespace ivm {

export auto compile_script(
	Napi::Env env,
	iv8::collected_external<agent>& agent,
	ivm::value::string_t code_string
) -> Napi::Value {
	auto& ienv = environment::get(env);
	auto [ dispatch, promise ] = make_promise<ivm::script>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::script> script) -> expected_value {
			return (value::accept<value::accept_pass, Napi::Value>{env})(
				iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*script))
			);
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

export auto run_script(
	Napi::Env env,
	iv8::collected_external<agent>& agent,
	iv8::collected_external<script>& script,
	iv8::collected_external<realm>& realm
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<value::value_t>(
		env,
		[](Napi::Env env, std::unique_ptr<value::value_t> result) -> expected_value {
			return value::transfer_strict<Napi::Value>(std::move(*result), env);
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

} // namespace ivm
