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

auto create_realm(
	environment& env,
	js::napi::untagged_external<agent_handle>& agent
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, isolated_v8::realm realm) -> auto {
			return js::forward{js::napi::untagged_external<realm_handle>::make(env, std::move(agent), std::move(realm))};
		}
	);
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto realm = isolated_v8::realm::make(lock);
			dispatch(std::move(agent), std::move(realm));
		},
		*agent
	);
	return js::forward{promise};
}

auto instantiate_runtime(
	environment& env,
	js::napi::untagged_external<realm_handle>& realm
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, isolated_v8::js_module module_) -> auto {
			return js::forward{js::napi::untagged_external<module_handle>::make(env, std::move(agent), std::move(module_))};
		}
	);
	realm->agent().schedule(
		[ dispatch = std::move(dispatch),
			realm = realm->realm() ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto module_ = realm.invoke(lock, [ & ](const isolated_v8::realm::scope& realm) -> isolated_v8::js_module {
				return lock->environment().runtime().instantiate(realm);
			});
			dispatch(std::move(agent), std::move(module_));
		},
		realm->agent()
	);
	return js::forward{promise};
}

realm_handle::realm_handle(agent_handle agent, isolated_v8::realm realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::make_create_realm(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function{create_realm});
}

auto realm_handle::make_instantiate_runtime(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function{instantiate_runtime});
}

} // namespace backend_napi_v8
