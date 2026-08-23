module backend_napi_v8;
import :environment;
import :lock;
import :realm;
import :utility;
import auto_js;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

module_handle::module_handle(
	agent_handle agent,
	js::iv8::shared_remote<js::iv8::module_record> module,
	std::optional<std::u16string> specifier,
	std::vector<js::iv8::module_request> requests
) :
		agent_{std::move(agent)},
		module_{std::move(module)},
		specifier_{std::move(specifier)},
		requests_{std::move(requests)} {}

auto module_handle::compile(
	const environment::lock& lock,
	agent_handle& agent,
	js::string_t source_text,
	compile_module_options options
) -> forward_promise_type {
	using expected_type = std::expected<module_handle, js::error_value>;
	auto [ promise, resolver ] = make_promise(
		lock,
		[](const environment::lock& lock, expected_type result) -> auto {
			return make_completion_record(result.transform([ & ](module_handle& module_) -> auto {
				return js::forward{class_template(lock)->construct(lock, std::move(module_))};
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
				auto maybe_module = iv8::unmaybe_one(lock, [ & ] -> v8::MaybeLocal<js::iv8::module_record> {
					return js::iv8::module_record::compile(lock, std::move(source_text), std::move(origin));
				});
				return std::move(maybe_module).transform([ & ](v8::Local<js::iv8::module_record> module_record) -> auto {
					auto shared_module = make_shared_remote(lock, module_record);
					auto requests = module_record->requests(lock);
					return module_handle{std::move(agent), shared_module, std::move(specifier), std::move(requests)};
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
	const environment::lock& lock,
	realm_handle& realm,
	capability_interface_type capability_interface,
	create_capability_options options
) -> forward_promise_type {
	auto [ promise, resolver ] = make_promise(
		lock,
		[](const environment::lock& lock, module_handle module_) -> auto {
			return js::forward{module_handle::class_template(lock)->construct(lock, std::move(module_))};
		}
	);

	// Makes `js::free_function` which invokes the user-supplied callback capability
	auto make_capability_callback = [ & ](forward_callback_type capability) -> auto {
		// Invoked in the node thread
		auto invoke =
			[ callback = js::napi::make_shared_remote(lock, *capability) ](
				const environment::lock& lock,
				js::values_vector_t params
			) -> void {
			callback->deref(lock)->apply(lock, std::move(params));
		};
		// Invoked in the isolate thread. An instance of this keeps the nodejs loop alive.
		return js::free_function{
			[ scheduler = lock->scheduler().make_ref(lock),
				invoke = std::move(invoke) ](
				const realm_scope& /*lock*/,
				js::rest /*rest*/,
				js::values_vector_t params
			) -> void {
				(*scheduler)(
					[ invoke ](napi_env nenv, napi_value /*nothing*/, js::values_vector_t params) -> void {
						auto& host_environment = napi::environment::unsafe_get_environment_as<environment>(nenv);
						auto lock = environment::lock{napi::environment_lock_witness::make_witness(host_environment), host_environment};
						// TODO(?): Exceptions here just fall back to process.on('uncaughtException').
						std::ignore = napi::invoke_internal_error_scope(lock, [ & ] -> napi_value {
							invoke(lock, std::move(params));
							return {};
						});
					},
					std::move(params)
				);
			},
		};
	};

	// Forward value interface as value
	constexpr auto forward_value_capability = util::overloaded{
		[](std::u16string value) -> auto { return value; },
		[](std::string value) -> auto { return value; },
	};

	// Apply `make_capability_callback` to each entry in the interface. This will be passed to
	// `create_synthetic` to instantiate the module.
	auto external_capability_interface = std::vector{
		std::from_range,
		std::move(capability_interface) |
			std::views::transform([ & ](auto pair) {
				auto [ key, value ] = std::move(pair);
				return std::pair{
					std::move(key),
					util::map_variant(std::move(value), util::overloaded{make_capability_callback, forward_value_capability}),
				};
			}),
	};

	// Make synthetic module
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
				external_capability_interface |
					std::views::as_rvalue |
					std::views::transform([ & ](auto pair) {
						auto [ key, value ] = std::move(pair);
						auto fn_template = std::visit(
							[ & ](auto capability) -> auto {
								return js::transfer_in<v8::Local<v8::Template>>(std::move(capability), lock);
							},
							std::move(value)
						);
						return std::pair{std::move(key), std::move(fn_template)};
					}),
			};
			auto module_record = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) -> auto {
				return js::iv8::module_record::create_synthetic(realm, std::move(options).origin, std::move(capability_interface));
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

auto module_handle::evaluate(const environment::lock& lock, realm_handle* realm) -> forward_promise_type {
	auto [ promise, resolver ] = make_promise(lock);
	if (realm == nullptr) {
		return js::forward{promise};
	}
	agent_.schedule(
		[](
			const agent_handle::lock& agent,
			auto resolver,
			const js::iv8::shared_remote<v8::Context>& realm,
			const js::iv8::shared_remote<js::iv8::module_record>& module_record
		) -> void {
			auto result = context_scope_operation(agent, realm->deref(agent), [ & ](const realm_scope& realm) -> auto {
				return module_record->deref(realm)->evaluate(realm);
			});
			resolver.resolve(make_completion_record(std::move(result)));
		},
		std::move(resolver),
		realm->realm(),
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
				std::views::transform([ & ](auto& remote_module) -> v8::Local<v8::Module> {
					return remote_module->deref(lock);
				}),
		},
		.payload = std::move(link_record).payload,
	};
};

auto module_handle::link(const environment::lock& lock, realm_handle* realm, module_handle_link_record link_record) -> forward_promise_type {
	auto scheduler = lock->scheduler();
	auto [ promise, resolver ] = make_promise(lock);
	if (realm == nullptr) {
		return js::forward{promise};
	}

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
			const js::iv8::shared_remote<js::iv8::module_record>& module,
			remote_module_link_record link_record
		) -> void {
			auto result = context_scope_operation(agent, realm->deref(agent), [ & ](const realm_scope& lock) -> auto {
				return iv8::invoke_externalized_error_scope(lock, [ & ] -> auto {
					auto module_local = module->deref(lock);
					auto local_link_record = deref_remote_link_record(lock, std::move(link_record));
					module_local->link(lock, std::move(local_link_record));
				});
			});
			// TODO: resolver should accept a `std::expected<T, E>`?
			if (result) {
				auto expected = *std::move(result);
				if (expected) {
					resolver.resolve(true);
				} else {
					resolver.reject(std::move(expected).error());
				}
			}
		},
		std::move(resolver),
		realm->realm(),
		module_,
		std::move(remote_link_record)
	);
	return js::forward{promise};
}

auto module_handle::requests(const environment::lock& /*lock*/) -> std::vector<js::iv8::module_request> {
	return requests_;
}

auto module_handle::specifier(const environment::lock& /*lock*/) -> std::optional<std::u16string> {
	return specifier_;
}

auto module_handle::class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<module_handle>> {
	return lock->class_template(
		std::type_identity<module_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Module">},
			js::class_method{util::cw<"_link">, util::fn<&module_handle::link>},
			js::class_getter{util::cw<"requests">, util::fn<&module_handle::requests>},
			js::class_getter{util::cw<"specifier">, util::fn<&module_handle::specifier>},
			js::class_method{util::cw<"evaluate">, util::fn<&module_handle::evaluate>},
		}
	);
}

} // namespace backend_napi_v8

// There is some spooky issue in clang v22.1.0 on macOS where if these templates are not explicitly
// instantiated in this file it causes bad codegen throughout the project. Like code totally
// unrelated to JS modules will throw vector out bounds exceptions, SIGBUS, asan violations. I tried
// disabling LTO, this is the only thing that seems to work.
[[maybe_unused]] constexpr auto template_workaround = [](js::iv8::context_lock_witness lock, js::value_t value) -> void {
	std::ignore = js::transfer_in_strict<v8::Local<v8::Value>>(std::move(value), lock);
};
