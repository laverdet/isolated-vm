module;
#include <optional>
#include <tuple>
#include <variant>
module backend_napi_v8;
import :environment;
import :utility;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
using namespace isolated_v8;
using namespace util::string_literals;

namespace backend_napi_v8 {

struct make_agent_options {
		struct clock_deterministic {
				js::js_clock::time_point epoch;
				double interval{};
		};
		struct clock_microtask {
				std::optional<js::js_clock::time_point> epoch;
		};
		struct clock_realtime {
				js::js_clock::time_point epoch;
		};
		struct clock_system {};
		using clock_type = std::variant<clock_deterministic, clock_microtask, clock_realtime, clock_system>;

		std::optional<clock_type> clock;
		std::optional<double> random_seed;
};

auto create_agent(environment& env, std::optional<make_agent_options> options_optional) {
	auto options = std::move(options_optional).value_or(make_agent_options{});
	auto& cluster = env.cluster();
	auto [ dispatch, promise ] = make_promise(
		env,
		[](environment& env, isolated_v8::agent agent) -> expected_value {
			return js::napi::untagged_external<isolated_v8::agent>::make(env, std::move(agent));
		}
	);
	auto clock = std::visit(
		util::overloaded{
			[](const make_agent_options::clock_deterministic& options) -> clock::any_clock {
				return clock::deterministic{options.epoch, js::js_clock::duration{options.interval}};
			},
			[](const make_agent_options::clock_microtask& options) -> clock::any_clock {
				return clock::microtask{options.epoch};
			},
			[](const make_agent_options::clock_realtime& options) -> clock::any_clock {
				return clock::realtime{options.epoch};
			},
			[](const make_agent_options::clock_system& /*clock*/) -> clock::any_clock {
				return clock::system{};
			},
		},
		options.clock.value_or(make_agent_options::clock_system{})
	);
	agent::make(
		[ dispatch = std::move(dispatch) ](
			isolated_v8::agent agent
		) mutable {
			dispatch(std::move(agent));
		},
		cluster,
		{.clock = clock, .random_seed = options.random_seed}
	);

	return js::transfer_direct{promise};
}

auto make_create_agent(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env, js::free_function<create_agent>{});
}

} // namespace backend_napi_v8

// Options visitors & acceptors
namespace js {
using backend_napi_v8::make_agent_options;

template <>
struct struct_properties<make_agent_options::clock_deterministic> {
		constexpr static auto properties = std::tuple{
			property{"epoch"_st, struct_member{&make_agent_options::clock_deterministic::epoch}},
			property{"interval"_st, struct_member{&make_agent_options::clock_deterministic::interval}},
		};
};

template <>
struct struct_properties<make_agent_options::clock_microtask> {
		constexpr static auto properties = std::tuple{
			property{"epoch"_st, struct_member{&make_agent_options::clock_microtask::epoch}},
		};
};

template <>
struct struct_properties<make_agent_options::clock_realtime> {
		constexpr static auto properties = std::tuple{
			property{"epoch"_st, struct_member{&make_agent_options::clock_realtime::epoch}},
		};
};

template <>
struct struct_properties<make_agent_options::clock_system> {
		constexpr static auto properties = std::tuple{};
};

template <>
struct union_of<make_agent_options::clock_type> {
		constexpr static auto& discriminant = "type";
		constexpr static auto alternatives = std::tuple{
			alternative<make_agent_options::clock_deterministic>{"deterministic"},
			alternative<make_agent_options::clock_microtask>{"microtask"},
			alternative<make_agent_options::clock_realtime>{"realtime"},
			alternative<make_agent_options::clock_system>{"system"},
		};
};

template <>
struct struct_properties<make_agent_options> {
		constexpr static auto properties = std::tuple{
			property{"clock"_st, struct_member{&make_agent_options::clock}},
			property{"randomSeed"_st, struct_member{&make_agent_options::random_seed}},
		};
};

} // namespace js
