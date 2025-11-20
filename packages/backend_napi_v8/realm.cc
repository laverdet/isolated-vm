module;
#include <utility>
module backend_napi_v8;
import :agent;
import :environment;
import :lock;
import :module_;
import :utility;
import auto_js;
import napi_js;
import v8_js;

namespace backend_napi_v8 {

realm_handle::realm_handle(agent_handle agent, js::iv8::shared_remote<v8::Context> realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::create(agent_handle& agent, environment& env) -> js::napi::value<js::promise_tag> {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Context> realm) -> auto {
			return js::forward{class_template(env).construct(env, std::move(agent), std::move(realm))};
		}
	);
	agent.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto realm = make_shared_remote(lock, lock->make_context());
			dispatch(std::move(agent), std::move(realm));
		},
		agent
	);
	return promise;
}

auto realm_handle::instantiate_runtime(environment& env) -> js::napi::value<js::promise_tag> {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Module> module_record) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_record))};
		}
	);
	agent_.schedule(
		[ dispatch = std::move(dispatch),
			realm = realm_ ](
			const agent_handle::lock& lock,
			agent_handle agent
		) -> void {
			auto module_record = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) -> auto {
				return make_shared_remote(lock, lock->environment().runtime().instantiate(realm));
			});
			dispatch(std::move(agent), module_record);
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
			js::class_method{util::cw<u8"createCapability">, make_forward_callback(module_handle::create_capability)},
			js::class_method{util::cw<u8"instantiateRuntime">, make_forward_callback(util::fn<&realm_handle::instantiate_runtime>)},
		}
	);
}

} // namespace backend_napi_v8
