module;
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module backend_napi_v8;
import :environment;
import :lock;
import :realm;
import :utility;
import auto_js;
import napi_js;
import util;
import v8_js;

namespace backend_napi_v8 {

module_handle::module_handle(agent_handle agent, js::iv8::shared_remote<v8::Module> module) :
		agent_{std::move(agent)},
		module_{std::move(module)} {}

auto module_handle::compile(
	agent_handle& agent,
	environment& env,
	js::string_t source_text,
	compile_module_options options
) -> js::forward<js::napi::value<>> {
	using value_type = std::tuple<module_handle, std::optional<std::u16string>, std::vector<js::iv8::module_request>>;
	using expected_type = std::expected<value_type, js::error_value>;
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, expected_type result) -> auto {
			return make_completion_record(env, result.transform([ & ](value_type& module_data) -> auto {
				auto& [ module_, specifier, requests ] = module_data;
				auto class_template = js::napi::value<class_tag_of<module_handle>>::from(env.module_class());
				return js::forward{class_template.runtime_construct(
					env,
					std::tuple{std::move(module_)},
					std::tuple{std::move(specifier), std::move(requests)}
				)};
			}));
		}
	);
	agent.schedule(
		[](
			const agent_handle::lock& lock,
			auto resolver,
			agent_handle agent,
			js::string_t source_text,
			compile_module_options options
		) -> void {
			auto origin = std::move(options).origin.value_or(js::iv8::source_origin{});
			auto specifier = origin.name;
			auto maybe_module_data = context_scope_operation(lock, lock->scratch_context(), [ & ](const realm_scope& lock) -> auto {
				auto maybe_module = js::iv8::module_record::compile(lock, std::move(source_text), std::move(origin));
				return std::move(maybe_module).transform([ & ](v8::Local<v8::Module> module_record) {
					auto shared_module = make_shared_remote(lock, module_record);
					auto requests = js::iv8::module_record::requests(lock, module_record);
					auto module_ = module_handle{std::move(agent), shared_module};
					return std::tuple{std::move(module_), std::move(specifier), std::move(requests)};
				});
			});
			resolver(std::move(maybe_module_data));
		},
		std::move(resolver),
		agent,
		std::move(source_text),
		std::move(options)
	);
	return js::forward{promise};
}

auto module_handle::create_capability(
	realm_handle& realm,
	environment& env,
	callback_type make_capability,
	create_capability_options options
) -> js::forward<js::napi::value<>> {
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
			const auto scope = js::napi::handle_scope{napi_env{env}};
			callback->deref(env).apply(env, std::move(params));
		};
		// Invoked in the isolate thread
		return js::free_function{
			// TODO: I don't like this `&env` capture, but `scheduler` should throw away the callback if
			// the environment isn't still valid.
			[ &env,
				scheduler = env.scheduler(),
				invoke = std::move(invoke) ](
				const realm_scope& /*lock*/,
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
				auto resolver
			) -> void {
			agent.schedule(
				[](
					const agent_handle::lock& lock,
					const js::iv8::shared_remote<v8::Context>& realm,
					auto resolver
				) -> void {
					context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) mutable -> void {
						resolver(realm);
					});
				},
				realm,
				std::move(resolver)
			);
		};
		return js::free_function{
			[ accept_subscriber = subscriber->take_subscriber(),
				schedule_task = std::move(schedule_task) ](
				const realm_scope& lock,
				js::forward<v8::Local<v8::Function>, function_tag> callback
			) -> void {
				auto callback_remote = make_shared_remote(lock, *callback);
				auto wake =
					[ schedule_task = std::move(schedule_task),
						callback_remote = std::move(callback_remote) ](
						js::value_t message
					) -> bool {
					schedule_task(
						[ callback_remote,
							message = std::move(message) ](
							const realm_scope& lock
						) mutable -> void {
							auto callback = callback_remote->deref(lock);
							auto argv = js::transfer_in_strict<v8::Local<v8::Value>>(std::move(message), lock);
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
	auto [ promise, resolver ] = make_promise(
		env,
		[](environment& env, module_handle module_) -> auto {
			return js::forward{module_handle::class_template(env).construct(env, std::move(module_))};
		}
	);
	realm.agent().schedule(
		[](
			const agent_handle::lock& lock,
			auto resolver,
			agent_handle agent,
			const auto& realm,
			create_capability_options options,
			auto external_capability_interface
		) -> void {
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
			auto module_record = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) mutable -> auto {
				return js::iv8::module_record::create_synthetic(realm, std::move(capability_interface), std::move(options).origin);
			});
			resolver(module_handle{std::move(agent), make_shared_remote(lock, std::move(module_record))});
		},
		std::move(resolver),
		realm.agent(),
		realm.realm(),
		std::move(options),
		std::move(external_capability_interface)
	);
	return js::forward{promise};
};

auto module_handle::evaluate(environment& env, realm_handle& realm) -> js::forward<js::napi::value<>> {
	auto [ promise, resolver ] = make_promise(env);
	agent_.schedule(
		[](
			const agent_handle::lock& agent,
			auto resolver,
			const js::iv8::shared_remote<v8::Context>& realm,
			const js::iv8::shared_remote<v8::Module>& module_record
		) -> void {
			context_scope_operation(agent, realm->deref(agent), [ & ](const realm_scope& realm) -> void {
				js::iv8::module_record::evaluate(realm, module_record->deref(realm));
			});
			resolver.resolve(std::monostate{});
		},
		std::move(resolver),
		realm.realm(),
		module_
	);
	return js::forward{promise};
}

// Deref `remote<v8::Module>` into `v8::Module`
auto deref_remote_link_record(js::iv8::isolate_lock_witness lock, remote_module_link_record&& link_record) -> js::iv8::module_link_record {
	return {
		.modules = std::vector{
			std::from_range,
			link_record.modules |
				std::views::transform([ & ](auto& remote_module) {
					return remote_module->deref(lock);
				}),
		},
		.payload = std::move(link_record).payload,
	};
};

auto module_handle::link(environment& env, realm_handle& realm, module_handle_link_record link_record) -> js::forward<js::napi::value<>> {
	auto scheduler = env.scheduler();
	auto [ promise, resolver ] = make_promise(env);

	// Convert `module_handle` to `remote<v8::Module>`
	auto remote_link_record = remote_module_link_record{
		.modules = std::vector{
			std::from_range,
			link_record.modules |
				std::views::transform([ & ](auto& module) {
					return module->module_;
				}),
		},
		.payload = std::move(link_record.payload),
	};

	// Schedule the link operation
	agent_.schedule(
		[](
			const agent_handle::lock& agent,
			auto resolver,
			const js::iv8::shared_remote<v8::Context>& realm,
			const js::iv8::shared_remote<v8::Module>& module,
			remote_module_link_record link_record
		) -> void {
			auto module_local = module->deref(agent);
			auto local_link_record = deref_remote_link_record(agent, std::move(link_record));
			context_scope_operation(agent, realm->deref(agent), [ & ](const realm_scope& lock) -> void {
				js::iv8::module_record::link(lock, module_local, std::move(local_link_record));
			});
			resolver.resolve(true);
		},
		std::move(resolver),
		realm.realm(),
		module_,
		std::move(remote_link_record)
	);
	return js::forward{promise};
}

auto module_handle::class_template(environment& env) -> js::napi::value<class_tag_of<module_handle>> {
	return env.class_template(
		std::type_identity<module_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Module">},
			js::class_method{util::cw<u8"_link">, util::fn<&module_handle::link>},
			js::class_method{util::cw<u8"evaluate">, util::fn<&module_handle::evaluate>},
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
	return class_template(env).transfer_construct(env, std::move(capability), std::tuple{});
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
