module;
#include <array>
#include <optional>
#include <tuple>
#include <variant>
module ivm.node;
import :environment;
import :external;
import :utility;
import :visit;
import ivm.isolated_v8;
import ivm.utility;
import ivm.iv8;
import ivm.value;
import napi;

namespace ivm {

struct make_agent_options {
		struct clock_deterministic {
				value::js_clock::time_point epoch;
				double interval{};
		};
		struct clock_microtask {
				std::optional<value::js_clock::time_point> epoch;
		};
		struct clock_realtime {
				value::js_clock::time_point epoch;
		};
		struct clock_system {};
		using clock_type = std::variant<clock_deterministic, clock_microtask, clock_realtime, clock_system>;

		std::optional<clock_type> clock;
		std::optional<double> random_seed;
};

auto create_agent(environment& env, std::optional<make_agent_options> options_optional) -> Napi::Value {
	auto options = std::move(options_optional).value_or(make_agent_options{});
	auto& cluster = env.cluster();
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, ivm::agent agent) -> expected_value {
		return make_collected_external<ivm::agent>(env, std::move(agent));
	});
	auto clock = std::visit(
		util::overloaded{
			[](const make_agent_options::clock_deterministic& options) -> clock::any_clock {
				return clock::deterministic{options.epoch, value::js_clock::duration{options.interval}};
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
	cluster.make_agent(
		[ dispatch = std::move(dispatch) ](
			ivm::agent agent,
			ivm::agent::lock&
		) mutable {
			dispatch(std::move(agent));
		},
		clock,
		options.random_seed
	);

	return promise;
}

auto create_realm(environment& env, iv8::external_reference<agent>& agent) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, ivm::realm realm) -> expected_value {
		return make_collected_external<ivm::realm>(env, std::move(realm));
	});
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			ivm::agent::lock& agent
		) mutable {
			auto realm = ivm::realm::make(agent);
			dispatch(std::move(realm));
		}
	);

	return promise;
}

auto make_create_agent(environment& env) -> Napi::Function {
	return make_node_function(env, create_agent);
}

auto make_create_realm(environment& env) -> Napi::Function {
	return make_node_function(env, create_realm);
}

} // namespace ivm

// Options visitors & acceptors
namespace ivm::value {

template <>
struct object_properties<make_agent_options::clock_deterministic> {
		constexpr static auto properties = std::tuple{
			member<"epoch", &make_agent_options::clock_deterministic::epoch>{},
			member<"interval", &make_agent_options::clock_deterministic::interval>{},
		};
};

template <>
struct object_properties<make_agent_options::clock_microtask> {
		constexpr static auto properties = std::tuple{
			member<"epoch", &make_agent_options::clock_microtask::epoch>{},
		};
};

template <>
struct object_properties<make_agent_options::clock_realtime> {
		constexpr static auto properties = std::tuple{
			member<"epoch", &make_agent_options::clock_realtime::epoch>{},
		};
};

template <>
struct object_properties<make_agent_options::clock_system> {
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
struct object_properties<make_agent_options> {
		constexpr static auto properties = std::tuple{
			member<"clock", &make_agent_options::clock, false>{},
			member<"randomSeed", &make_agent_options::random_seed, false>{},
		};
};

} // namespace ivm::value
