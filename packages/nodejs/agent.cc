module;
#include <concepts>
#include <memory>
#include <optional>
#include <variant>
module ivm.node;
import :environment;
import :utility;
import :visit;
import ivm.isolated_v8;
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

auto create_agent(Napi::Env env, environment& ienv, make_agent_options options) -> Napi::Value {
	auto& cluster = ienv.cluster();
	auto [ dispatch, promise ] = make_promise<ivm::agent>(
		env,
		ienv.collection(),
		[ &ienv ](Napi::Env env, ivm::agent agent) -> expected_value {
			auto agent_handle = iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(agent));
			return value::direct_cast<Napi::Value>(agent_handle, env, ienv);
		}
	);
	auto clock = std::visit(
		overloaded{
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

auto create_realm(Napi::Env env, environment& ienv, iv8::collected_external<agent>& agent) -> Napi::Value {
	auto [ dispatch, promise ] = make_promise<ivm::realm>(
		env,
		ienv.collection(),
		[ &ienv ](Napi::Env env, ivm::realm realm) -> expected_value {
			auto realm_handle = iv8::make_collected_external(ienv.collection(), ienv.isolate(), std::move(realm));
			return value::direct_cast<Napi::Value>(realm_handle, env, ienv);
		}
	);
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

auto make_create_agent(Napi::Env env, ivm::environment& ienv) -> Napi::Function {
	return make_node_function(env, ienv, ivm::create_agent);
}

auto make_create_realm(Napi::Env env, ivm::environment& ienv) -> Napi::Function {
	return make_node_function(env, ienv, ivm::create_realm);
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
