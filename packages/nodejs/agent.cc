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

export auto create_agent(Napi::Env env, environment& ienv) -> Napi::Value {
	auto& cluster = environment::get(env).cluster();
	auto [ dispatch, promise ] = make_promise<ivm::agent>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::agent> agent) -> expected_value {
			auto agent_handle = iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*agent));
			return value::direct_cast<Napi::Value>(agent_handle, env, ienv);
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

export auto create_realm(Napi::Env env, environment& ienv, iv8::collected_external<agent>& agent) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<ivm::realm>(
		env,
		[ &ienv ](Napi::Env env, std::unique_ptr<ivm::realm> realm) -> expected_value {
			auto realm_handle = iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(*realm));
			return value::direct_cast<Napi::Value>(realm_handle, env, ienv);
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
