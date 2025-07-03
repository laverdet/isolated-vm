module;
#include <future>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module backend_napi_v8.module_;
import backend_napi_v8.environment;
import backend_napi_v8.realm;
import backend_napi_v8.utility;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
import v8_js;
using namespace isolated_v8;
using namespace util::string_literals;

namespace backend_napi_v8 {

struct compile_module_options {
		std::optional<source_origin> origin;
};

struct create_capability_options {
		source_required_name origin;
};

auto compile_module(
	environment& env,
	js::napi::untagged_external<agent>& agent,
	js::string_t source_text,
	std::optional<compile_module_options> options_optional
) {
	auto options = std::move(options_optional).value_or(compile_module_options{});
	auto [ dispatch, promise ] = make_promise(
		env,
		[](
			environment& env,
			isolated_v8::js_module module_,
			std::vector<isolated_v8::module_request> requests
		) -> expected_value {
			auto* handle = js::napi::untagged_external<isolated_v8::js_module>::make(env, std::move(module_));
			auto result = std::tuple{
				js::transfer_direct{handle},
				std::move(requests),
			};
			return js::transfer_in_strict<napi_value>(std::move(result), env);
		}
	);
	agent->schedule(
		[ options = std::move(options),
			source_text = std::move(source_text),
			dispatch = std::move(dispatch) ](
			isolated_v8::agent::lock& agent
		) mutable {
			auto origin = std::move(options.origin).value_or(source_origin{});
			auto module = isolated_v8::js_module::compile(agent, std::move(source_text), origin);
			auto requests = module.requests(agent);
			dispatch(std::move(module), requests);
		}
	);
	return js::transfer_direct{promise};
}

auto evaluate_module(
	environment& env,
	js::napi::untagged_external<agent>& agent,
	js::napi::untagged_external<realm_handle>& realm,
	js::napi::untagged_external<js_module>& module_
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, js::value_t result) -> expected_value {
			return js::transfer_in_strict<napi_value>(std::move(result), env);
		}
	);
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			isolated_v8::agent::lock& agent,
			isolated_v8::realm realm,
			isolated_v8::js_module module_
		) mutable {
			isolated_v8::realm::scope realm_scope{agent, realm};
			auto result = module_.evaluate(realm_scope);
			dispatch(std::move(result));
		},
		realm->realm(),
		*module_
	);
	return js::transfer_direct{promise};
}

auto link_module(
	environment& env,
	js::napi::untagged_external<agent>& agent,
	js::napi::untagged_external<realm_handle>& realm,
	js::napi::untagged_external<js_module>& module_,
	js::napi::value<js::function_tag> link_callback_
) {
	js::napi::reference callback{env, link_callback_};
	auto scheduler = env.scheduler();
	js::napi::reference link_callback_ref{env, link_callback_};
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env) -> expected_value {
			return js::napi::invoke(napi_get_boolean, napi_env{env}, true);
		}
	);
	agent->schedule_async(
		[ &env,
			link_callback_ref = std::move(link_callback_ref),
			scheduler = std::move(scheduler),
			dispatch = std::move(dispatch) ](
			const std::stop_token& /*stop_token*/,
			isolated_v8::agent::lock& agent,
			isolated_v8::realm realm,
			isolated_v8::js_module module
		) mutable {
			isolated_v8::realm::scope realm_scope{agent, realm};
			module.link(
				realm_scope,
				[ & ](
					auto specifier,
					auto referrer,
					auto attributes
				) -> isolated_v8::js_module& {
					std::promise<isolated_v8::js_module*> promise;
					auto future = promise.get_future();
					scheduler(
						[ & ]() {
							js::napi::handle_scope scope{napi_env{env}};
							auto callback = js::bound_function{
								[ &promise ](
									environment& /*env*/,
									js::napi::value<js::value_tag> /*error*/,
									js::napi::untagged_external<js_module>* module
								) -> void {
									if (module) {
										promise.set_value(&**module);
									}
								}
							};
							auto callback_fn = js::napi::value<js::function_tag>::make(env, std::move(callback));
							link_callback_ref.get(env).call<std::monostate>(
								env,
								std::move(specifier),
								std::move(referrer),
								std::move(attributes),
								js::transfer_direct{callback_fn}
							);
						}
					);
					return *future.get();
				}
			);
			dispatch();
		},
		realm->realm(),
		*module_
	);
	return js::transfer_direct{promise};
}

auto create_capability(
	environment& env,
	js::napi::untagged_external<isolated_v8::agent>& agent,
	js::napi::value<js::function_tag> capability,
	create_capability_options options
) {
	// Callback to invoke the capability in the node environment
	auto capability_callback =
		[ callback = js::napi::make_shared_remote(env, capability) ](
			environment& env,
			std::vector<js::value_t> params
		) {
			js::napi::handle_scope scope{napi_env{env}};
			callback->get(env).apply<std::monostate>(env, std::move(params));
		};
	// Callback passed to isolated v8
	auto invoke_capability = js::bound_function{
		[ &env,
			capability_callback = std::move(capability_callback) ](
			realm::scope& /*realm*/,
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
		[](environment& env, js_module module_) -> expected_value {
			return js::napi::untagged_external<isolated_v8::js_module>::make(env, std::move(module_));
		}
	);
	agent->schedule(
		[ invoke_capability = std::move(invoke_capability),
			dispatch = std::move(dispatch),
			options = std::move(options) ](
			isolated_v8::agent::lock& agent
		) mutable {
			auto exports = isolated_v8::function_template::make(agent, std::move(invoke_capability));
			auto module_ = isolated_v8::js_module::create_synthetic(agent, exports, std::move(options.origin));
			dispatch(std::move(module_));
		}
	);
	return js::transfer_direct{promise};
}

auto make_compile_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<compile_module>{});
}

auto make_create_capability(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<create_capability>{});
}

auto make_evaluate_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<evaluate_module>{});
}

auto make_link_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<link_module>{});
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
