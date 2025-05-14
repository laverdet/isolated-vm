module;
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
module backend_napi_v8.script;
import backend_napi_v8.environment;
import backend_napi_v8.external;
import backend_napi_v8.utility;
import isolated_v8;
import v8_js;
import isolated_js;
import napi_js;
import nodejs;
using namespace isolated_v8;

namespace backend_napi_v8 {

struct compile_script_options {
		std::optional<source_origin> origin;
};

auto compile_script(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::string_t code_string,
	std::optional<compile_script_options> options_optional
) {
	auto options = std::move(options_optional).value_or(compile_script_options{});
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, isolated_v8::script script) -> expected_value {
			return make_external<isolated_v8::script>(env, std::move(script));
		}
	);
	agent->schedule(
		[ code_string = std::move(code_string),
			options = std::move(options),
			dispatch = std::move(dispatch) ](
			isolated_v8::agent::lock& agent
		) mutable {
			auto origin = std::move(options.origin).value_or(source_origin{});
			dispatch(isolated_v8::script::compile(agent, std::move(code_string), std::move(origin)));
		}
	);
	return js::transfer_direct{promise};
}

auto run_script(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::iv8::external_reference<script>& script,
	js::iv8::external_reference<realm>& realm
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
			isolated_v8::script script
		) mutable {
			isolated_v8::realm::scope realm_scope{agent, realm};
			auto result = script.run(realm_scope);
			dispatch(std::move(result));
		},
		*realm,
		*script
	);
	return js::transfer_direct{promise};
}

auto make_compile_script(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<compile_script>{});
}

auto make_run_script(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<run_script>{});
}

} // namespace backend_napi_v8

namespace js {
using backend_napi_v8::compile_script_options;

template <>
struct struct_properties<compile_script_options> {
		constexpr static auto properties = std::tuple{
			property<"origin">::descriptor{struct_member{&compile_script_options::origin}},
		};
};

} // namespace js
