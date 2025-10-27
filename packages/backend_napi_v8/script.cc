module;
#include <expected>
#include <optional>
#include <stop_token>
#include <tuple>
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
using namespace isolated_v8;

namespace backend_napi_v8 {

struct compile_script_options {
		std::optional<source_origin> origin;
};

struct run_script_options {
		std::optional<double> timeout;
};

auto compile_script(
	environment& env,
	js::napi::untagged_external<agent_handle>& agent,
	js::string_t code_string,
	compile_script_options options
) {
	using expected_type = std::expected<isolated_v8::script_shared_remote_type, js::error_value>;
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, expected_type script) -> auto {
			return make_completion_record(env, std::move(script).transform([ & ](isolated_v8::script_shared_remote_type script) {
				return js::forward{script_handle::class_template(env).construct(env, std::move(script))};
			}));
		}
	);
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			const agent_handle::lock& agent,
			js::string_t code_string,
			compile_script_options options
		) -> void {
			auto origin = std::move(options.origin).value_or(source_origin{});
			auto local = isolated_v8::compile_script(agent, std::move(code_string), std::move(origin));
			dispatch(local.transform(transform_shared_remote(agent)));
		},
		std::move(code_string),
		std::move(options)
	);
	return js::forward{promise};
}

auto run_script(
	js::tagged_external_of<script_handle>& script_handle,
	environment& env,
	js::napi::untagged_external<realm_handle>& realm,
	run_script_options options
) {
	using expected_type = std::expected<js::value_t, js::error_value>;
	auto [ dispatch, promise ] =
		make_promise(env, [](environment& env, expected_type result) -> auto {
			return make_completion_record(env, std::move(result));
		});
	realm->agent().schedule(
		[ options = options,
			realm = realm->realm(),
			script_remote = script_handle.script() ](
			const agent_handle::lock& agent,
			auto dispatch
		) -> void {
			auto result = realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) -> expected_type {
				auto stop_token =
					options.timeout.transform([ & ](double timeout) -> util::timer_stop_token {
						return util::timer_stop_token{js::js_clock::duration{timeout}};
					});
				auto timer_callback =
					stop_token.transform([ & ](util::timer_stop_token& timer) {
						return std::stop_callback{
							*timer, [ & ]() {
								agent->isolate()->TerminateExecution();
							}
						};
					});
				return isolated_v8::run_script(realm, script_remote->deref(agent));
			});
			dispatch(std::move(result));
		},
		std::move(dispatch)
	);
	return js::forward{promise};
}

auto make_compile_script(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function{compile_script});
}

auto script_handle::class_template(environment& env) -> js::napi::value<class_tag_of<script_handle>> {
	return env.class_template(
		std::type_identity<script_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Script">},
			js::class_method{util::cw<u8"run">, run_script},
		}
	);
}

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::compile_script_options;
using backend_napi_v8::run_script_options;

template <>
struct struct_properties<compile_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{util::cw<"origin">, struct_member{&compile_script_options::origin}},
		};
};

template <>
struct struct_properties<run_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{util::cw<"timeout">, struct_member{&run_script_options::timeout}},
		};
};

} // namespace js
