module;
#include "runtime/dist/runtime.js.h"
#include <exception>
#include <utility>
module backend_napi_v8;
import :environment;
import :utility;
import isolated_js;
import isolated_v8;
import napi_js;

namespace backend_napi_v8 {

auto create_realm(
	environment& env,
	js::napi::untagged_external<isolated_v8::agent>& agent
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, isolated_v8::agent agent, isolated_v8::realm realm) -> expected_value {
			return js::napi::untagged_external<realm_handle>::make(env, std::move(agent), std::move(realm));
		}
	);
	agent->schedule(
		[ agent = *agent,
			dispatch = std::move(dispatch) ](
			isolated_v8::agent::lock& lock
		) mutable {
			auto realm = isolated_v8::realm::make(lock);
			auto runtime = isolated_v8::js_module::compile(lock, runtime_dist_runtime_js, isolated_v8::source_origin{});
			isolated_v8::realm::scope realm_scope{lock, realm};
			runtime.link(realm_scope, [](auto&&...) -> isolated_v8::js_module& {
				std::terminate();
			});
			runtime.evaluate(realm_scope);
			dispatch(std::move(agent), std::move(realm));
		}
	);
	return js::transfer_direct{promise};
}

realm_handle::realm_handle(isolated_v8::agent agent, isolated_v8::realm realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::make_create_realm(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<create_realm>{});
}

} // namespace backend_napi_v8
