module;
#include <future>
#include <optional>
#include <stop_token>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>
module ivm.node;
import :environment;
import :external;
import :utility;
import ivm.isolated_v8;
import ivm.iv8;
import ivm.js;
import ivm.napi;
import napi;

namespace ivm {

struct compile_module_options {
		std::optional<source_origin> origin;
};

auto compile_module(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::string_t source_text,
	std::optional<compile_module_options> options_optional
) {
	auto options = std::move(options_optional).value_or(compile_module_options{});
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, ivm::js_module module_, std::vector<ivm::module_request> requests) -> expected_value {
			auto* handle = make_collected_external<ivm::js_module>(env, std::move(module_));
			return js::transfer_strict<napi_value>(
				std::tuple{
					js::transfer_direct{handle},
					std::move(requests),
				},
				std::tuple{},
				std::tuple{env.nenv()}
			);
		}
	);
	agent->schedule(
		[ options = std::move(options),
			source_text = std::move(source_text),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			auto origin = std::move(options.origin).value_or(source_origin{});
			auto module = ivm::js_module::compile(agent, std::move(source_text), origin);
			auto requests = module.requests(agent);
			dispatch(std::move(module), requests);
		}
	);
	return js::transfer_direct{promise};
}

auto evaluate_module(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::iv8::external_reference<realm>& realm,
	js::iv8::external_reference<js_module>& module_
) {
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, js::value_t result) -> expected_value {
		return js::transfer_strict<napi_value>(std::move(result), std::tuple{}, std::tuple{env.nenv()});
	});
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent,
			ivm::realm realm,
			ivm::js_module module_
		) mutable {
			ivm::realm::managed_scope realm_scope{agent, realm};
			auto result = module_.evaluate(realm_scope);
			dispatch(std::move(result));
		},
		*realm,
		*module_
	);
	return js::transfer_direct{promise};
}

auto link_module(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::iv8::external_reference<realm>& realm_,
	js::iv8::external_reference<js_module>& module_,
	js::napi::value<js::function_tag> link_callback_
) {
	auto* nenv = env.nenv();
	auto callback = js::napi::reference{nenv, link_callback_};
	auto scheduler = env.scheduler();
	js::napi::reference link_callback_ref{nenv, link_callback_};
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env) -> expected_value {
			return js::napi::invoke(napi_get_boolean, env.nenv(), true);
		}
	);
	agent->schedule_async(
		[ &env,
			link_callback_ref = std::move(link_callback_ref),
			scheduler = std::move(scheduler),
			dispatch = std::move(dispatch) ](
			const std::stop_token& /*stop_token*/,
			ivm::agent::lock& agent,
			ivm::realm realm,
			ivm::js_module module
		) mutable {
			ivm::realm::managed_scope realm_scope{agent, realm};
			module.link(
				realm_scope,
				[ & ](
					auto specifier,
					auto referrer,
					auto attributes
				) -> ivm::js_module& {
					std::promise<ivm::js_module*> promise;
					auto future = promise.get_future();
					scheduler.schedule(
						[ & ]() {
							js::napi::handle_scope scope{env.nenv()};
							auto callback =
								[](
									std::promise<js_module*>& promise,
									js::napi::value<js::value_tag> /*error*/,
									js::iv8::external_reference<js_module>* module
								) -> void {
								if (module) {
									promise.set_value(&**module);
								}
							};
							auto callback_fn = js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<callback>{}, promise);
							auto link_callback = js::napi::function{env.nenv(), *link_callback_ref};
							link_callback.invoke<std::monostate>(
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
		*realm_,
		*module_
	);
	return js::transfer_direct{promise};
}

auto make_compile_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<compile_module>{}, env);
}

auto make_evaluate_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<evaluate_module>{}, env);
}

auto make_link_module(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<link_module>{}, env);
}

} // namespace ivm

namespace ivm::js {

template <>
struct object_properties<compile_module_options> {
		constexpr static auto properties = std::tuple{
			member<"origin", &compile_module_options::origin, false>{},
		};
};

} // namespace ivm::js
