module;
#include <utility>
module backend_napi_v8;
import :agent;
import :environment;
import :module_;
import :utility;
import isolated_js;
import isolated_v8;
import napi_js;

namespace backend_napi_v8 {

realm_handle::realm_handle(agent_handle agent, isolated_v8::realm realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::create(agent_handle& agent, environment& env) -> js::napi::value<js::promise_tag> {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, isolated_v8::realm realm) -> auto {
			return js::forward{class_template(env).construct(env, std::move(agent), std::move(realm))};
		}
	);
	agent.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto realm = isolated_v8::realm::make(lock);
			dispatch(std::move(agent), std::move(realm));
		},
		agent
	);
	return promise;
}

auto realm_handle::instantiate_runtime(environment& env) -> js::napi::value<js::promise_tag> {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, isolated_v8::js_module module_) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_))};
		}
	);
	agent_.schedule(
		[ dispatch = std::move(dispatch),
			realm = realm_ ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto module_ = realm.invoke(lock, [ & ](const isolated_v8::realm::scope& realm) -> isolated_v8::js_module {
				return lock->environment().runtime().instantiate(realm);
			});
			dispatch(std::move(agent), std::move(module_));
		},
		agent_
	);
	return promise;
}

auto realm_handle::class_template(environment& env) -> js::napi::value<class_tag_of<realm_handle>> {
	return env.class_template(
		std::type_identity<realm_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Realm">},
			js::class_method{util::cw<u8"instantiateRuntime">, make_forward_callback(util::fn<&realm_handle::instantiate_runtime>)},
		}
	);
}

} // namespace backend_napi_v8
