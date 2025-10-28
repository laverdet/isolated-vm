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

namespace backend_napi_v8 {

module_handle::module_handle(agent_handle agent, isolated_v8::js_module module) :
		agent_{std::move(agent)},
		module_{std::move(module)} {}

auto module_handle::compile(agent_handle& agent, environment& env, js::string_t source_text, compile_module_options options) -> js::napi::value<promise_tag> {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](
			environment& env,
			agent_handle agent,
			isolated_v8::js_module module_,
			std::vector<isolated_v8::module_request> requests
		) -> auto {
			auto handle = module_handle::class_template(env).construct(env, std::move(agent), std::move(module_));
			return std::tuple{
				js::forward{handle},
				std::move(requests),
			};
		}
	);
	agent.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent,
			js::string_t source_text,
			compile_module_options options
		) -> void {
			auto origin = std::move(options).origin.value_or(source_origin{});
			auto module = isolated_v8::js_module::compile(lock, std::move(source_text), std::move(origin));
			auto requests = module.requests(lock);
			dispatch(std::move(agent), std::move(module), std::move(requests));
		},
		agent,
		std::move(source_text),
		std::move(options)
	);
	return promise;
}

auto module_handle::create_capability(agent_handle& agent, environment& env, callback_type capability, create_capability_options options) -> js::napi::value<promise_tag> {
	// Callback to invoke the capability in the node environment
	auto capability_callback =
		[ callback = js::napi::make_shared_remote(env, *capability) ](
			environment& env,
			std::vector<js::value_t> params
		) -> void {
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
		) -> void {
			env.scheduler()(
				[ &env,
					capability_callback,
					params = std::move(params) ]() mutable -> void {
					capability_callback(env, std::move(params));
				}
			);
		}
	};
	// Make synthetic module
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js_module module_) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_))};
		}
	);
	agent.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent,
			create_capability_options options,
			auto invoke_capability
		) {
			auto make_interface = [ & ]() -> auto {
				return std::make_tuple(
					std::pair{util::cw<"default">, isolated_v8::function_template::make(lock, std::move(invoke_capability))}
				);
			};
			auto module_ = isolated_v8::js_module::create_synthetic(lock, make_interface(), std::move(options).origin);
			dispatch(std::move(agent), std::move(module_));
		},
		agent,
		std::move(options),
		std::move(invoke_capability)
	);
	return promise;
}

auto module_handle::evaluate(environment& env, realm_handle& realm) -> js::napi::value<promise_tag> {
	auto [ dispatch, promise ] = make_promise(env, [](environment& /*env*/, js::value_t result) -> js::value_t {
		return result;
	});
	agent_.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& agent,
			isolated_v8::realm realm,
			isolated_v8::js_module module_
		) -> void {
			auto result = realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) -> js::value_t {
				return module_.evaluate(realm);
			});
			dispatch(std::move(result));
		},
		realm.realm(),
		module_
	);
	return promise;
}

auto module_handle::link(environment& env, realm_handle& realm, callback_type link_callback) -> js::napi::value<promise_tag> {
	using attributes_type = isolated_v8::module_request::attributes_type;
	auto scheduler = env.scheduler();
	js::napi::reference link_callback_ref{env, *link_callback};
	auto [ dispatch, promise ] = make_promise(env, [](environment& /*env*/) -> bool { return true; });
	// Invoke the passed link callback in response to v8 link callback. This is invoked in the node
	// environment.
	auto clink_link_callback =
		[ &env,
			link_callback_ref = std::move(link_callback_ref) ](
			std::promise<isolated_v8::js_module> promise,
			js::string_t specifier,
			std::optional<js::string_t> referrer,
			attributes_type attributes
		) -> void {
		js::napi::handle_scope scope{napi_env{env}};
		auto callback = js::free_function{
			[ promise = std::move(promise) ](
				environment& /*env*/,
				js::forward<js::napi::value<js::value_tag>> /*error*/,
				module_handle* module
			) mutable -> void {
				if (module) {
					promise.set_value(module->module_);
				} else {
					throw std::logic_error{"link failure"};
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
			js::string_t specifier,
			std::optional<js::string_t> referrer,
			attributes_type attributes
		) -> isolated_v8::js_module {
		std::promise<isolated_v8::js_module> promise;
		auto future = promise.get_future();
		scheduler([ & ]() -> void {
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
	agent_.schedule_async(
		[ dispatch = std::move(dispatch) ](
			const std::stop_token& /*stop_token*/,
			const agent_handle::lock& agent,
			const isolated_v8::realm& realm,
			isolated_v8::js_module module,
			auto host_link_callback
		) -> void {
			realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) -> void {
				module.link(realm, std::move(host_link_callback));
			});
			dispatch();
		},
		realm.realm(),
		module_,
		std::move(host_link_callback)
	);
	return promise;
}

auto module_handle::class_template(environment& env) -> js::napi::value<class_tag_of<module_handle>> {
	return env.class_template(
		std::type_identity<module_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Module">},
			js::class_method{util::cw<u8"evaluate">, make_forward_callback(util::fn<&module_handle::evaluate>)},
			js::class_method{util::cw<u8"link">, make_forward_callback(util::fn<&module_handle::link>)},
		}
	);
}

} // namespace backend_napi_v8
