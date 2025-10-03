module;
#include <future>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module backend_napi_v8;
import :environment;
import :realm;
import :utility;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
using namespace isolated_v8;
using namespace util::string_literals;

namespace backend_napi_v8 {

struct compile_module_options {
		std::optional<source_origin> origin;
};

struct create_capability_options {
		source_required_name origin;
};

module_handle::module_handle(agent_handle agent, isolated_v8::js_module module) :
		agent_{std::move(agent)},
		module_{std::move(module)} {}

auto compile_module(
	environment& env,
	js::napi::untagged_external<agent_handle>& agent,
	js::string_t source_text,
	std::optional<compile_module_options> options_optional
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](
			environment& env,
			agent_handle agent,
			isolated_v8::js_module module_,
			std::vector<isolated_v8::module_request> requests
		) -> expected_value {
			auto* handle = js::napi::untagged_external<module_handle>::make(env, std::move(agent), std::move(module_));
			auto result = std::tuple{
				js::forward{handle},
				std::move(requests),
			};
			return js::transfer_in_strict<napi_value>(std::move(result), env);
		}
	);
	agent->schedule(
		[ agent = *agent,
			dispatch = std::move(dispatch),
			source_text = std::move(source_text),
			options = std::move(options_optional).value_or(compile_module_options{}) ](
			const agent_handle::lock& lock
		) mutable {
			auto origin = std::move(options).origin.value_or(source_origin{});
			auto module = isolated_v8::js_module::compile(lock, std::move(source_text), std::move(origin));
			auto requests = module.requests(lock);
			dispatch(std::move(agent), std::move(module), std::move(requests));
		}
	);
	return js::forward{promise};
}

auto evaluate_module(
	environment& env,
	js::napi::untagged_external<realm_handle>& realm,
	js::napi::untagged_external<module_handle>& module_
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, js::value_t result) -> expected_value {
			return js::transfer_in_strict<napi_value>(std::move(result), env);
		}
	);
	module_->agent().schedule(
		[ dispatch = std::move(dispatch),
			realm = realm->realm(),
			module_ = module_->module() ](
			const agent_handle::lock& agent
		) mutable {
			auto result = realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) -> js::value_t {
				return module_.evaluate(realm);
			});
			dispatch(std::move(result));
		}
	);
	return js::forward{promise};
}

auto link_module(
	environment& env,
	js::napi::untagged_external<realm_handle>& realm,
	js::napi::untagged_external<module_handle>& module_,
	js::forward<js::napi::value<js::function_tag>> link_callback
) {
	auto scheduler = env.scheduler();
	js::napi::reference link_callback_ref{env, *link_callback};
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env) -> expected_value {
			return js::napi::invoke(napi_get_boolean, napi_env{env}, true);
		}
	);
	// Invoke the passed link callback in response to v8 link callback. This is invoked in the node
	// environment.
	auto clink_link_callback =
		[ &env,
			link_callback_ref = std::move(link_callback_ref) ](
			std::promise<isolated_v8::js_module> promise,
			auto specifier,
			auto referrer,
			auto attributes
		) {
			js::napi::handle_scope scope{napi_env{env}};
			auto callback = js::free_function{
				[ promise = std::move(promise) ](
					environment& /*env*/,
					js::forward<js::napi::value<js::value_tag>> /*error*/,
					js::napi::untagged_external<module_handle>* module
				) mutable -> void {
					if (module) {
						promise.set_value((*module)->module());
					}
				}
			};
			auto callback_fn = js::napi::value<js::function_tag>::make(env, std::move(callback));
			link_callback_ref.get(env).call<std::monostate>(
				env,
				std::move(specifier),
				std::move(referrer),
				std::move(attributes),
				js::forward{callback_fn}
			);
		};
	// Invoked by v8 to resolve a module link.
	auto host_link_callback =
		[ scheduler = std::move(scheduler),
			clink_link_callback = std::move(clink_link_callback) ](
			auto specifier,
			auto referrer,
			auto attributes
		) mutable -> isolated_v8::js_module {
		std::promise<isolated_v8::js_module> promise;
		auto future = promise.get_future();
		scheduler([ & ]() {
			clink_link_callback(
				std::move(promise),
				std::move(specifier),
				std::move(referrer),
				std::move(attributes)
			);
		});
		return future.get();
	};
	// Schedule the link operation
	module_->agent().schedule_async(
		[ host_link_callback = std::move(host_link_callback),
			dispatch = std::move(dispatch) ](
			const std::stop_token& /*stop_token*/,
			const agent_handle::lock& agent,
			isolated_v8::realm realm,
			isolated_v8::js_module module
		) mutable {
			realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) {
				module.link(realm, std::move(host_link_callback));
			});
			dispatch();
		},
		realm->realm(),
		module_->module()
	);
	return js::forward{promise};
}

auto create_capability(
	environment& env,
	js::napi::untagged_external<agent_handle>& agent,
	js::forward<js::napi::value<js::function_tag>, function_tag> capability,
	create_capability_options options
) {
	// Callback to invoke the capability in the node environment
	auto capability_callback =
		[ callback = js::napi::make_shared_remote(env, *capability) ](
			environment& env,
			std::vector<js::value_t> params
		) {
			js::napi::handle_scope scope{napi_env{env}};
			callback->get(env).apply<std::monostate>(env, std::move(params));
		};
	// Callback passed to isolated v8
	auto invoke_capability = js::free_function{
		[ &env,
			capability_callback = std::move(capability_callback) ](
			const realm::scope& /*realm*/,
			js::rest /*rest*/,
			std::vector<js::value_t> params
		) mutable {
			env.scheduler()(
				[ &env,
					capability_callback,
					params = std::move(params) ]() mutable {
					capability_callback(env, std::move(params));
				}
			);
		}
	};
	// Make synthetic module
	auto [ dispatch, promise ] = make_promise(
		env,
		[](
			environment& env,
			agent_handle agent,
			js_module module_
		) -> expected_value {
			return js::napi::untagged_external<module_handle>::make(env, std::move(agent), std::move(module_));
		}
	);
	agent->schedule(
		[ invoke_capability = std::move(invoke_capability),
			dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent,
			create_capability_options options
		) mutable {
			auto make_interface = [ & ]() {
				return std::make_tuple(
					std::pair{"default"_sl, isolated_v8::function_template::make(lock, std::move(invoke_capability))}
				);
			};
			auto module_ = isolated_v8::js_module::create_synthetic(lock, make_interface(), std::move(options).origin);
			dispatch(std::move(agent), std::move(module_));
		},
		*agent,
		std::move(options)
	);
	return js::forward{promise};
}

auto module_handle::make_compile_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<compile_module>());
}

auto module_handle::make_create_capability(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<create_capability>());
}

auto module_handle::make_evaluate_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<evaluate_module>());
}

auto module_handle::make_link_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<link_module>());
}

} // namespace backend_napi_v8

namespace js {
using namespace backend_napi_v8;

template <>
struct struct_properties<compile_module_options> {
		constexpr static auto properties = std::tuple{
			property{"origin"_st, struct_member{&compile_module_options::origin}},
		};
};

template <>
struct struct_properties<create_capability_options> {
		constexpr static auto properties = std::tuple{
			property{"origin"_st, struct_member{&create_capability_options::origin}},
		};
};

} // namespace js
