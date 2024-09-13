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
import ivm.v8;
import ivm.value;
import napi;
import v8;

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

auto create_agent(environment& env, make_agent_options options) -> Napi::Value {
	auto& cluster = env.cluster();
	auto [ dispatch, promise ] = make_promise<ivm::agent>(env, [](environment& env, ivm::agent agent) -> expected_value {
		return make_collected_external<ivm::agent>(env, std::move(agent));
	});
	auto clock = std::visit(
		util::overloaded{
			[](const make_agent_options::clock_deterministic& options) -> agent::clock::any_clock {
				return agent::clock::deterministic{options.epoch, value::js_clock::duration{options.interval}};
			},
			[](const make_agent_options::clock_microtask& options) -> agent::clock::any_clock {
				return agent::clock::microtask{options.epoch};
			},
			[](const make_agent_options::clock_realtime& options) -> agent::clock::any_clock {
				return agent::clock::realtime{options.epoch};
			},
			[](const make_agent_options::clock_system& /*clock*/) -> agent::clock::any_clock {
				return agent::clock::system{};
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
	auto [ dispatch, promise ] = make_promise<ivm::realm>(env, [](environment& env, ivm::realm realm) -> expected_value {
		return make_collected_external<ivm::realm>(env, std::move(realm));
	});
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			ivm::agent::lock& lock
		) mutable {
			auto realm = ivm::realm::make(lock);
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

template <class Meta, class Value>
struct object_map<Meta, Value, make_agent_options::clock_deterministic> : object_properties<Meta, Value, make_agent_options::clock_deterministic> {
		template <auto Member>
		using property = object_properties<Meta, Value, make_agent_options::clock_deterministic>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{true, "epoch", property<&make_agent_options::clock_deterministic::epoch>::accept},
			std::tuple{true, "interval", property<&make_agent_options::clock_deterministic::interval>::accept},
		};
};

template <class Meta, class Value>
struct object_map<Meta, Value, make_agent_options::clock_microtask> : object_properties<Meta, Value, make_agent_options::clock_microtask> {
		template <auto Member>
		using property = object_properties<Meta, Value, make_agent_options::clock_microtask>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{true, "epoch", property<&make_agent_options::clock_microtask::epoch>::accept},
		};
};

template <class Meta, class Value>
struct object_map<Meta, Value, make_agent_options::clock_realtime> : object_properties<Meta, Value, make_agent_options::clock_realtime> {
		template <auto Member>
		using property = object_properties<Meta, Value, make_agent_options::clock_realtime>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{true, "epoch", property<&make_agent_options::clock_realtime::epoch>::accept},
		};
};

template <class Meta, class Value>
struct object_map<Meta, Value, make_agent_options::clock_system> : object_no_properties<Meta, Value, make_agent_options::clock_system> {};

template <class Meta, class Value>
struct discriminated_union<Meta, Value, make_agent_options::clock_type> : discriminated_alternatives<Meta, Value, make_agent_options::clock_type> {
		template <class Type>
		constexpr static auto alternative = &discriminated_alternatives<Meta, Value, make_agent_options::clock_type>::template accept<Type>;

		constexpr static auto discriminant = "type";
		constexpr static auto alternatives = std::array{
			std::pair{"deterministic", alternative<make_agent_options::clock_deterministic>},
			std::pair{"microtask", alternative<make_agent_options::clock_microtask>},
			std::pair{"realtime", alternative<make_agent_options::clock_realtime>},
			std::pair{"system", alternative<make_agent_options::clock_system>},
		};
};

template <class Meta, class Value>
struct object_map<Meta, Value, make_agent_options> : object_properties<Meta, Value, make_agent_options> {
		template <auto Member>
		using property = object_properties<Meta, Value, make_agent_options>::template property<Member>;

		constexpr static auto properties = std::array{
			std::tuple{false, "clock", property<&make_agent_options::clock>::accept},
			std::tuple{false, "randomSeed", property<&make_agent_options::random_seed>::accept},
		};
};

} // namespace ivm::value
