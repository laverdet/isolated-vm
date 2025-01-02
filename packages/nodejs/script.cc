module;
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
module ivm.node;
import :environment;
import :external;
import :utility;
import ivm.isolated_v8;
import ivm.iv8;
import ivm.napi;
import ivm.value;
import napi;

namespace ivm {

struct compile_script_options {
		std::optional<source_origin> origin;
};

auto compile_script(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::string_t code_string,
	std::optional<compile_script_options> options_optional
) -> napi_value {
	auto options = std::move(options_optional).value_or(compile_script_options{});
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, ivm::script script) -> expected_value {
		return make_collected_external<ivm::script>(env, std::move(script));
	});
	agent->schedule(
		[ code_string = std::move(code_string),
			options = std::move(options),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			auto origin = std::move(options.origin).value_or(source_origin{});
			dispatch(ivm::script::compile(agent, std::move(code_string), std::move(origin)));
		}
	);
	return promise;
}

auto run_script(
	environment& env,
	js::iv8::external_reference<agent>& agent,
	js::iv8::external_reference<script>& script,
	js::iv8::external_reference<realm>& realm
) -> napi_value {
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, js::value_t result) -> expected_value {
		return js::transfer_strict<napi_value>(std::move(result), std::tuple{}, std::tuple{env.nenv()});
	});
	agent->schedule(
		[ &realm,
			&script,
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			ivm::realm::managed_scope realm_scope{agent, *realm};
			auto result = script->run(realm_scope);
			dispatch(std::move(result));
		}
	);
	return promise;
}

auto make_compile_script(environment& env) -> napi_value {
	return make_node_function<compile_script>(env);
}

auto make_run_script(environment& env) -> napi_value {
	return make_node_function<run_script>(env);
}

} // namespace ivm

namespace ivm::js {

template <>
struct object_properties<compile_script_options> {
		constexpr static auto properties = std::tuple{
			member<"origin", &compile_script_options::origin, false>{},
		};
};

} // namespace ivm::js
