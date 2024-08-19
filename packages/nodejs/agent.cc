module;
#include <memory>
export module ivm.node:agent;
import :environment;
import :make_promise;
import :visit;
import ivm.isolated_v8;
import ivm.v8;
import ivm.value;
import napi;
import v8;

namespace ivm {

export auto create_agent(Napi::Env env) -> Napi::Value {
	auto& ienv = environment::get(env);
	auto& cluster = environment::get(env).cluster();
	auto [ dispatch, promise ] = make_promise<ivm::agent>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::agent> agent) -> expected_value {
			return (value::accept<value::accept_pass, Napi::Value>{env})(
				iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*agent))
			);
		}
	);
	cluster.make_agent(
		[ dispatch = std::move(dispatch) ](
			ivm::agent agent,
			ivm::agent::lock&
		) mutable {
			dispatch(std::move(agent));
		}
	);

	return promise;
}

export auto create_realm(Napi::Env env, iv8::collected_external<agent>& agent) -> Napi::Value {
	auto& ienv = environment::get(env);
	auto [ dispatch, promise ] = make_promise<ivm::realm>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::realm> realm) -> expected_value {
			return (value::accept<value::accept_pass, Napi::Value>{env})(
				iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*realm))
			);
		}
	);
	agent->schedule_task(
		[ dispatch = std::move(dispatch) ](
			ivm::agent::lock& lock
		) mutable {
			auto realm = ivm::realm::make(lock);
			dispatch(std::move(realm));
		}
	);

	return promise;
}

} // namespace ivm
