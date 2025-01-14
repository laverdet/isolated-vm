module;
#include "runtime/dist/runtime.js.h"
#include <optional>
#include <tuple>
#include <variant>
module ivm.node;
import :environment;
import :external;
import :utility;
import ivm.isolated_v8;
import ivm.iv8;
import ivm.js;
import ivm.napi;
import ivm.utility;
import napi;
using namespace isolated_v8;

namespace ivm {

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
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, isolated_v8::agent agent) -> expected_value {
		return make_external<isolated_v8::agent>(env, std::move(agent));
	});
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
	cluster.make_agent(
		[ dispatch = std::move(dispatch) ](
			isolated_v8::agent agent
		) mutable {
			dispatch(std::move(agent));
		},
		clock,
		options.random_seed
	);

	return js::transfer_direct{promise};
}

auto create_realm(environment& env, js::iv8::external_reference<agent>& agent) {
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, isolated_v8::realm realm) -> expected_value {
		return make_external<isolated_v8::realm>(env, std::move(realm));
	});
	agent->schedule(
		[ dispatch = std::move(dispatch) ](
			isolated_v8::agent::lock& agent
		) mutable {
			auto realm = isolated_v8::realm::make(agent);
			auto runtime = isolated_v8::js_module::compile(agent, runtime_dist_runtime_js, source_origin{});
			isolated_v8::realm::managed_scope realm_scope{agent, realm};
			runtime.link(realm_scope, [](auto&&...) -> isolated_v8::js_module& {
				std::terminate();
			});
			runtime.evaluate(realm_scope);
			dispatch(std::move(realm));
		}
	);

	return js::transfer_direct{promise};
}

auto make_create_agent(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<create_agent>{}, env);
}

auto make_create_realm(environment& env) -> js::napi::value<js::function_tag> {
	return js::napi::value<js::function_tag>::make(env.nenv(), js::free_function<create_realm>{}, env);
}

} // namespace ivm

// Options visitors & acceptors
namespace js {

using ivm::make_agent_options;

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

} // namespace js
