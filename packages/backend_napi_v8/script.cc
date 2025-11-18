module;
#include <expected>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
module backend_napi_v8;
import :environment;
import :realm;
import :utility;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
import v8_js;
using namespace isolated_v8;
namespace v8 = embedded_v8;

namespace backend_napi_v8 {

auto script_handle::compile_script(agent_handle& agent, environment& env, js::string_t code_string, compile_script_options options) -> js::napi::value<promise_tag> {
	using expected_type = std::expected<js::iv8::shared_remote<v8::UnboundScript>, js::error_value>;
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, expected_type script) -> auto {
			return make_completion_record(env, std::move(script).transform([ & ](js::iv8::shared_remote<v8::UnboundScript> script) -> auto {
				return js::forward{script_handle::class_template(env).construct(env, std::move(script))};
			}));
		}
	);
	agent.schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& agent,
			js::string_t code_string,
			compile_script_options options
		) -> void {
			auto origin = std::move(options).origin.value_or(js::iv8::source_origin{});
			auto maybe_script = context_scope_operation(agent, agent->scratch_context(), [ & ](const isolated_v8::realm_scope& lock) -> auto {
				auto maybe_script = js::iv8::script::compile(util::slice{lock}, std::move(code_string), std::move(origin));
				return maybe_script.transform([ & ](v8::Local<v8::UnboundScript> script) -> auto {
					return make_shared_remote(lock, script);
				});
			});
			dispatch(std::move(maybe_script));
		},
		std::move(code_string),
		std::move(options)
	);
	return promise;
}

auto script_handle::run(environment& env, realm_handle& realm, run_script_options options) -> js::napi::value<promise_tag> {
	using expected_type = std::expected<js::value_t, js::error_value>;
	auto [ dispatch, promise ] =
		make_promise(env, [](environment& env, expected_type result) -> auto {
			return make_completion_record(env, std::move(result));
		});
	realm.agent().schedule(
		[ options = options,
			realm = realm.realm(),
			script_remote = script_ ](
			const agent_handle::lock& lock,
			const auto& dispatch
		) -> void {
			auto result = context_scope_operation(lock, realm->deref(lock), [ & ](const isolated_v8::realm_scope& realm) -> expected_type {
				auto stop_token =
					options.timeout.transform([ & ](double timeout) -> util::timer_stop_token {
						return util::timer_stop_token{js::js_clock::duration{timeout}};
					});
				auto timer_callback =
					stop_token.transform([ & ](util::timer_stop_token& timer) {
						return std::stop_callback{
							timer.get_token(), [ & ]() {
								lock->isolate()->TerminateExecution();
							}
						};
					});
				return js::iv8::script::run(realm, script_remote->deref(lock));
			});
			dispatch(std::move(result));
		},
		std::move(dispatch)
	);
	return promise;
}

auto script_handle::class_template(environment& env) -> js::napi::value<class_tag_of<script_handle>> {
	return env.class_template(
		std::type_identity<script_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Script">},
			js::class_method{util::cw<u8"run">, make_forward_callback(util::fn<&script_handle::run>)},
		}
	);
}

} // namespace backend_napi_v8
