export module backend_napi_v8:agent;
import :agent_handle;
import :module_;
import :script;
import auto_js;
import napi_js;
import std;

namespace backend_napi_v8 {

// Value held by napi
export class agent_handle_value {
	public:
		struct create_options;
		struct clock_deterministic;
		struct clock_microtask;
		struct clock_realtime;
		struct clock_system;

		explicit agent_handle_value(agent_handle agent) : agent_{std::move(agent)} {}
		auto create_realm(environment& env) -> forward_promise_type;
		auto compile_module(environment& env, js::string_t source_text, compile_module_options options) -> forward_promise_type;
		auto compile_script(environment& env, js::string_t source_text, compile_script_options options) -> forward_promise_type;
		static auto create(environment& env, std::optional<create_options> options_optional) -> forward_promise_type;
		static auto class_template(environment& env) -> js::napi::value_of<class_tag_of<agent_handle_value>>;

	private:
		agent_handle agent_;
};

// Clock options
struct agent_handle_value::clock_deterministic {
		js::js_clock::time_point epoch;
		double interval{};

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"epoch">, &clock_deterministic::epoch},
			js::struct_member{util::cw<"interval">, &clock_deterministic::interval},
		};
};

struct agent_handle_value::clock_microtask {
		std::optional<js::js_clock::time_point> epoch;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"epoch">, &clock_microtask::epoch},
		};
};
struct agent_handle_value::clock_realtime {
		js::js_clock::time_point epoch;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"epoch">, &clock_realtime::epoch},
		};
};

struct agent_handle_value::clock_system {
		constexpr static auto struct_template = js::struct_template{};
};

// `Agent.create()` options
struct agent_handle_value::create_options {
		using clock_type = std::variant<clock_deterministic, clock_microtask, clock_realtime, clock_system>;

		std::optional<clock_type> clock;
		std::optional<double> random_seed;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"clock">, &create_options::clock},
			js::struct_member{util::cw<"randomSeed">, &create_options::random_seed},
		};
};

} // namespace backend_napi_v8

// Options visitors & acceptors
namespace js {
using backend_napi_v8::agent_handle_value;

template <>
struct transfer_type<agent_handle_value> : std::type_identity<js::tagged_external<agent_handle_value>> {};

template <>
struct union_of<agent_handle_value::create_options::clock_type> {
		constexpr static auto& discriminant = util::cw<"type">;
		constexpr static auto alternatives = std::tuple{
			alternative<agent_handle_value::clock_deterministic>{"deterministic"},
			alternative<agent_handle_value::clock_microtask>{"microtask"},
			alternative<agent_handle_value::clock_realtime>{"realtime"},
			alternative<agent_handle_value::clock_system>{"system"},
		};
};

} // namespace js
