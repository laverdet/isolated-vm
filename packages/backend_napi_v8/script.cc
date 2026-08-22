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

auto script_handle::compile_script(const environment::lock& lock, agent_handle& agent, js::string_t code_string, compile_script_options options) -> forward_promise_type {
	using expected_type = std::expected<script_handle, js::error_value>;
	auto [ promise, resolver ] = make_promise(
		lock,
		[](const environment::lock& lock, expected_type script) -> auto {
			return make_completion_record(script.transform([ & ](script_handle& script) -> auto {
				return js::forward{script_handle::class_template(lock)->construct(lock, std::move(script))};
			}));
		}
	);
	agent.schedule(
		[](
			const agent_handle::lock& agent,
			auto resolver,
			js::string_t code_string,
			compile_script_options options
		) -> void {
			auto origin = std::move(options).origin.value_or(js::iv8::source_origin{});
			auto maybe_script = context_scope_operation(agent, agent->scratch_context(), [ & ](const realm_scope& lock) -> auto {
				auto maybe_script = iv8::unmaybe_one(lock, [ & ] -> v8::MaybeLocal<js::iv8::script> {
					return js::iv8::script::compile(util::slice(lock), std::move(code_string), std::move(origin));
				});
				return maybe_script.transform([ & ](v8::Local<js::iv8::script> script) -> auto {
					return script_handle{make_shared_remote(lock, script)};
				});
			});
			resolver(std::move(maybe_script));
		},
		std::move(resolver),
		std::move(code_string),
		std::move(options)
	);
	return js::forward{promise};
}

auto script_handle::run(const environment::lock& lock, realm_handle* realm, run_script_options options) -> forward_promise_type {
	auto [ promise, resolver ] = make_promise(lock);
	if (realm == nullptr) {
		return js::forward{promise};
	}
	realm->agent().schedule(
		[ options = options,
			realm = realm->realm(),
			script_remote = script_ ](
			const agent_handle::lock& lock,
			auto resolver
		) -> void {
			auto result = context_scope_operation(lock, realm->deref(lock), [ & ](const realm_scope& realm) -> auto {
				auto stop_token =
					options.timeout.transform([ & ](double timeout) -> util::timer_stop_token {
						return util::timer_stop_token{js::js_clock::duration{timeout}};
					});
				auto timer_callback =
					stop_token.transform([ & ](util::timer_stop_token& timer) -> auto {
						return std::stop_callback{
							timer.get_token(), [ & ] -> auto {
								lock->executor().isolate()->TerminateExecution();
							}
						};
					});
				return iv8::invoke_externalized_error_scope(realm, [ & ] -> auto {
					auto value = script_remote->deref(lock)->run(realm);
					return js::transfer_out<js::value_t>(value, realm);
				});
			});
			if (result) {
				resolver.resolve(make_completion_record(std::move(*result)));
			}
		},
		std::move(resolver)
	);
	return js::forward{promise};
}

auto script_handle::class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<script_handle>> {
	return lock->class_template(
		std::type_identity<script_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Script">},
			js::class_method{util::cw<"run">, util::fn<&script_handle::run>},
		}
	);
}

} // namespace backend_napi_v8
