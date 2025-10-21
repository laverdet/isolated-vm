module;
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
using namespace util::string_literals;

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
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, isolated_v8::script script) -> expected_value {
			return js::napi::untagged_external<isolated_v8::script>::make(env, std::move(script));
		}
	);
	agent->schedule(
		[ code_string = std::move(code_string),
			options = std::move(options),
			dispatch = std::move(dispatch) ](
			const agent_handle::lock& agent
		) mutable -> void {
			auto origin = std::move(options.origin).value_or(source_origin{});
			dispatch(isolated_v8::script::compile(agent, std::move(code_string), std::move(origin)));
		}
	);
	return js::forward{promise};
}

auto run_script(
	environment& env,
	js::napi::untagged_external<script>& script,
	js::napi::untagged_external<realm_handle>& realm,
	run_script_options options
) {
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, js::value_t result) -> expected_value {
			return js::transfer_in_strict<napi_value>(std::move(result), env);
		}
	);
	realm->agent().schedule(
		[ options = options,
			realm = realm->realm(),
			script = *script ](
			const agent_handle::lock& agent,
			auto dispatch
		) -> void {
			auto result = realm.invoke(agent, [ & ](const isolated_v8::realm::scope& realm) -> js::value_t {
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
				return script.run(realm);
			});
			dispatch(std::move(result));
		},
		std::move(dispatch)
	);
	return js::forward{promise};
}

auto make_compile_script(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<compile_script>());
}

auto make_run_script(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::make_static_function<run_script>());
}

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::compile_script_options;
using backend_napi_v8::run_script_options;

template <>
struct struct_properties<compile_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{"origin"_st, struct_member{&compile_script_options::origin}},
		};
};

template <>
struct struct_properties<run_script_options> {
		constexpr static auto defaultable = true;
		constexpr static auto properties = std::tuple{
			property{"timeout"_st, struct_member{&run_script_options::timeout}},
		};
};

} // namespace js
