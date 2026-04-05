module backend_napi_v8;
import :agent;
import :environment;
import :lock;
import :module_;
import :reference;
import :utility;
import auto_js;
import napi_js;
import std;
import v8_js;

namespace backend_napi_v8 {

realm_handle::realm_handle(agent_handle agent, js::iv8::shared_remote<v8::Context> realm) :
		agent_{std::move(agent)},
		realm_{std::move(realm)} {}

auto realm_handle::create(agent_handle& agent, environment& env) -> js::forward<js::napi::value<>> {
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Context> realm) -> auto {
			return js::forward{class_template(env).construct(env, std::move(agent), std::move(realm))};
		}
	);
	agent.schedule(
		[](
			const agent_handle::lock& lock,
			auto resolver,
			agent_handle agent
		) -> void {
			auto realm = make_shared_remote(lock, lock->make_context());
			resolver(std::move(agent), std::move(realm));
		},
		std::move(resolver),
		agent
	);
	return js::forward{promise};
}

auto realm_handle::acquire_global_object(environment& env) -> js::forward<js::napi::value<>> {
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, reference_handle reference) -> auto {
			return js::forward{reference_handle::class_template(env).construct(env, std::move(reference))};
		}
	);
	agent_.schedule(
		[](
			const agent_handle::lock& lock,
			auto resolver,
			agent_handle agent,
			js::iv8::shared_remote<v8::Context> realm_remote
		) -> void {
			auto realm_local = realm_remote->deref(lock);
			auto global = realm_local->Global();
			resolver(reference_handle{lock, std::move(agent), std::move(realm_remote), global});
		},
		std::move(resolver),
		agent_,
		realm_
	);
	return js::forward{promise};
}

auto realm_handle::instantiate_runtime(environment& env) -> js::forward<js::napi::value<>> {
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js::iv8::shared_remote<v8::Module> module_record) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_record))};
		}
	);
	agent_.schedule(
		[ realm = realm_ ](
			const agent_handle::lock& lock,
			auto resolver,
			agent_handle agent
		) -> void {
			auto module_record = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) -> auto {
				return make_shared_remote(lock, lock->environment().runtime().instantiate(realm));
			});
			resolver(std::move(agent), module_record);
		},
		std::move(resolver),
		agent_
	);
	return js::forward{promise};
}

auto realm_handle::class_template(environment& env) -> js::napi::value<class_tag_of<realm_handle>> {
	return env.class_template(
		std::type_identity<realm_handle>{},
		js::class_template{
			js::class_constructor{util::cw<"Realm">},
			js::class_method{util::cw<"acquireGlobalObject">, util::fn<&realm_handle::acquire_global_object>},
			js::class_method{util::cw<"createCapability">, util::fn<&module_handle::create_capability>},
			js::class_method{util::cw<"instantiateRuntime">, util::fn<&realm_handle::instantiate_runtime>},
		}
	);
}

} // namespace backend_napi_v8
