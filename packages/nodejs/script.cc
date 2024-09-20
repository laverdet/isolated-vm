module;
#include <optional>
#include <utility>
#include <variant>
module ivm.node;
import :environment;
import :external;
import :script;
import :utility;
import :visit;
import ivm.isolated_v8;
import ivm.v8;
import ivm.value;
import napi;

namespace ivm {

struct compile_script_options {
		std::optional<source_origin_options> origin;
};

auto compile_script(
	environment& env,
	iv8::external_reference<agent>& agent,
	value::string_t code_string,
	std::optional<compile_script_options> options_optional
) -> Napi::Value {
	auto options = std::move(options_optional).value_or(compile_script_options{});
	auto [ dispatch, promise ] = make_promise<ivm::script>(env, [](environment& env, ivm::script script) -> expected_value {
		return make_collected_external<ivm::script>(env, std::move(script));
	});
	agent->schedule(
		[ code_string = std::move(code_string),
			options = std::move(options),
			dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			auto source_origin = make_source_origin(agent, options.origin);
			dispatch(ivm::script::compile(agent, std::move(code_string), source_origin));
		}
	);
	return promise;
}

auto run_script(
	environment& env,
	iv8::external_reference<agent>& agent,
	iv8::external_reference<script>& script,
	iv8::external_reference<realm>& realm
) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<value::value_t>(env, [](environment& env, value::value_t result) -> expected_value {
		return value::transfer_strict<Napi::Value>(std::move(result), env.napi_env());
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

auto make_source_origin(agent::lock& agent, const std::optional<source_origin_options>& options) -> source_origin {
	if (options) {
		auto location = options->location.value_or(source_location_options{});
		return {agent, options->name, {location.line, location.column}};
	} else {
		return {};
	}
}

auto make_compile_script(environment& env) -> Napi::Function {
	return make_node_function(env, compile_script);
}

auto make_run_script(environment& env) -> Napi::Function {
	return make_node_function(env, run_script);
}

} // namespace ivm

namespace ivm::value {

template <class Meta, class Value>
struct object_map<Meta, Value, compile_script_options> : object_properties<Meta, Value, compile_script_options> {
		template <auto Member>
		using property = object_properties<Meta, Value, compile_script_options>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "origin", property<&compile_script_options::origin>::accept},
		};
};

} // namespace ivm::value
