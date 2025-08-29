module;
#include "runtime/dist/runtime.js.h"
#include <exception>
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
		[](environment& env, isolated_v8::agent_handle agent, isolated_v8::realm realm) -> expected_value {
			return js::napi::untagged_external<realm_handle>::make(env, std::move(agent), std::move(realm));
		}
	);
	agent->agent().schedule(
		[ agent = *agent,
			dispatch = std::move(dispatch) ](
			const isolated_v8::agent_lock& lock
		) mutable {
			auto realm = isolated_v8::realm::make(lock);
			auto runtime = isolated_v8::js_module::compile(lock, runtime_dist_runtime_js, isolated_v8::source_origin{});
			realm.invoke(lock, [ & ](const isolated_v8::realm::scope& realm) {
				runtime.link(realm, [](auto&&...) -> isolated_v8::js_module& {
					std::terminate();
				});
				runtime.evaluate(realm);
			});
			dispatch(agent.agent(), std::move(realm));
		}
	);
	return js::forward{promise};
}

realm_handle::realm_handle(isolated_v8::agent_handle agent, isolated_v8::realm realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::make_create_realm(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<create_realm>());
}

} // namespace backend_napi_v8
