module;
#include <future>
#include <optional>
#include <ranges>
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
namespace v8 = embedded_v8;

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
			auto origin = std::move(options.origin).value_or(source_origin{});
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

auto module_handle::create_capability(
	realm_handle& realm,
	environment& env,
	callback_type make_capability,
	create_capability_options options
) -> js::napi::value<promise_tag> {
	// Make the `subscriber_capability` and pass it to the interface maker
	using capability_type = std::variant<callback_type, js::tagged_external<subscriber_capability>>;
	using capability_interface_type = js::dictionary<js::dictionary_tag, js::string_t, capability_type>;
	auto subscriber = subscriber_capability::make(env);
	auto local_capability_interface = make_capability->call<capability_interface_type>(env, js::forward{subscriber});

	// Makes `js::free_function` which invokes the user-supplied callback capability
	auto make_capability_callback = [ & ](callback_type capability) -> auto {
		// Invoked in the node thread
		auto invoke =
			[ callback = js::napi::make_shared_remote(env, *capability) ](
				environment& env,
				std::vector<js::value_t> params
			) -> void {
			js::napi::handle_scope scope{napi_env{env}};
			callback->get(env).apply(env, std::move(params));
		};
		// Invoked in the isolate thread
		return js::free_function{
			// TODO: I don't like this `&env` capture, but `scheduler` should throw away the callback if
			// the environment isn't still valid.
			[ &env,
				scheduler = env.scheduler(),
				invoke = std::move(invoke) ](
				const realm::scope& /*realm*/,
				js::rest /*rest*/,
				std::vector<js::value_t> params
			) -> void {
				scheduler(
					[ &env, invoke ](std::vector<js::value_t> params) -> void {
						invoke(env, std::move(params));
					},
					std::move(params)
				);
			},
		};
	};

	// Makes `js::free_function` from a `subscriber_capability`
	auto make_subscribe_capability = [ & ](js::tagged_external<subscriber_capability> subscriber) -> auto {
		// Wakes the isolate and runs the callback in the current realm
		auto schedule_task =
			[ agent = realm.agent(),
				realm = realm.realm() ](
				auto dispatch
			) -> void {
			agent.schedule(
				[](
					const agent_handle::lock& lock,
					isolated_v8::realm realm,
					auto dispatch
				) -> void {
					realm.invoke(lock, [ dispatch = std::move(dispatch) ](const isolated_v8::realm::scope& realm) mutable -> void {
						std::move(dispatch)(realm);
					});
				},
				realm,
				std::move(dispatch)
			);
		};
		return js::free_function{
			[ accept_subscriber = subscriber->take_subscriber(),
				schedule_task = std::move(schedule_task) ](
				const realm::scope& lock,
				js::forward<v8::Local<v8::Function>, function_tag> callback
			) -> void {
				auto callback_remote = js::iv8::make_shared_remote(lock, *callback);
				auto wake =
					[ schedule_task = std::move(schedule_task),
						callback_remote = std::move(callback_remote) ](
						js::value_t message
					) -> bool {
					schedule_task(
						[ callback_remote,
							message = std::move(message) ](
							const realm::scope& lock
						) -> void {
							auto callback = callback_remote.deref(lock);
							auto argv = js::transfer_in<v8::Local<v8::Value>>(std::move(message), lock);
							js::iv8::unmaybe(callback->Call(lock.context(), v8::Undefined(lock.isolate()), 1, &argv));
						}
					);
					return true;
				};
				accept_subscriber->subscribe(std::move(wake));
			},
		};
	};

	// Apply `make_capability_callback` and `make_subscribe_capability` to each entry in the
	// interface. This will be passed to `create_synthetic` to instantiate the module.
	auto external_capability_interface = std::vector{
		std::from_range,
		std::move(local_capability_interface) |
			std::views::transform([ & ](auto pair) {
				auto [ key, value ] = std::move(pair);
				return std::pair{
					std::move(key),
					util::map_variant(std::move(value), util::overloaded{make_capability_callback, make_subscribe_capability}),
				};
			}),
	};

	// Make synthetic module
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, agent_handle agent, js_module module_) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(agent), std::move(module_))};
		}
	);
	realm.agent().schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& lock,
			agent_handle agent,
			auto realm,
			create_capability_options options,
			auto external_capability_interface
		) {
			auto capability_interface = std::vector{
				std::from_range,
				std::move(external_capability_interface) |
					std::views::transform([ & ](auto pair) {
						auto [ key, value ] = std::move(pair);
						auto fn_template = std::visit(
							[ & ](auto capability) -> auto {
								return js::iv8::function_template::make(lock, std::move(capability));
							},
							std::move(value)
						);
						return std::pair{std::move(key), std::move(fn_template)};
					}),
			};
			auto module_ = realm.invoke(lock, [ & ](const isolated_v8::realm::scope& realm) mutable -> auto {
				return isolated_v8::js_module::create_synthetic(realm, std::move(capability_interface), std::move(options.origin));
			});
			dispatch(std::move(agent), std::move(module_));
		},
		realm.agent(),
		realm.realm(),
		std::move(options),
		std::move(external_capability_interface)
	);
	return promise;
};

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
		link_callback_ref.get(env).call(
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

// subscriber_capability
auto subscriber_capability::accept_callback(callback_type callback) -> void {
	*callback_.write() = std::move(callback);
}

auto subscriber_capability::take_subscriber() -> std::shared_ptr<subscriber> {
	if (!subscriber_) {
		throw js::runtime_error{u"Subscriber capability already forwarded"};
	} else {
		return std::move(subscriber_);
	}
}

auto subscriber_capability::send(js::value_t message) -> bool {
	auto lock = callback_.read();
	if (*lock) {
		return (*lock)(std::move(message));
	} else {
		return false;
	}
}

auto subscriber_capability::make(environment& env) -> js::napi::value<js::object_tag> {
	auto capability = std::make_shared<subscriber_capability>(private_constructor{});
	capability->subscriber_ = std::make_shared<subscriber>(capability);
	return class_template(env).transfer(env, std::move(capability));
}

auto subscriber_capability::class_template(environment& env) -> js::napi::value<js::class_tag_of<subscriber_capability>> {
	return env.class_template(
		std::type_identity<subscriber_capability>{},
		js::class_template{
			js::class_constructor{util::cw<u8"SubscriberCapability">},
			js::class_method{util::cw<u8"send">, util::fn<&subscriber_capability::send>},
		}
	);
}

// `subscriber_capability`
subscriber_capability::subscriber::subscriber(const std::shared_ptr<subscriber_capability>& capability) :
		capability_{capability} {}

auto subscriber_capability::subscriber::subscribe(callback_type callback) -> void {
	if (subscribed_) {
		throw js::runtime_error{u"Already subscribed"};
	}
	if (auto capability = capability_.lock()) {
		// No repeat subscriptions
		subscribed_ = true;
		capability_.reset();
		capability->accept_callback(std::move(callback));
	}
}

} // namespace backend_napi_v8
