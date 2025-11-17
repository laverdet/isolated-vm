module;
#include <optional>
#include <tuple>
#include <utility>
#include <variant>
module backend_napi_v8;
import :agent;
import :environment;
import :module_;
import :realm;
import :script;
import :utility;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;

namespace backend_napi_v8 {

struct make_agent_options {
		struct clock_deterministic {
				js::js_clock::time_point epoch;
				double interval{};

				constexpr static auto struct_template = js::struct_template{
					js::struct_member{util::cw<"epoch">, &make_agent_options::clock_deterministic::epoch},
					js::struct_member{util::cw<"interval">, &make_agent_options::clock_deterministic::interval},
				};
		};
		struct clock_microtask {
				std::optional<js::js_clock::time_point> epoch;

				constexpr static auto struct_template = js::struct_template{
					js::struct_member{util::cw<"epoch">, &make_agent_options::clock_microtask::epoch},
				};
		};
		struct clock_realtime {
				js::js_clock::time_point epoch;

				constexpr static auto struct_template = js::struct_template{
					js::struct_member{util::cw<"epoch">, &make_agent_options::clock_realtime::epoch},
				};
		};
		struct clock_system {
				constexpr static auto struct_template = js::struct_template{};
		};
		using clock_type = std::variant<clock_deterministic, clock_microtask, clock_realtime, clock_system>;

		std::optional<clock_type> clock;
		std::optional<double> random_seed;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"clock">, &make_agent_options::clock},
			js::struct_member{util::cw<"randomSeed">, &make_agent_options::random_seed},
		};
};

agent_environment::agent_environment(const isolated_v8::agent_lock& lock) :
		runtime_interface_{lock} {}

auto create_agent(environment& env, js::forward<js::napi::value<function_tag>> constructor, std::optional<make_agent_options> options_optional) {
	using namespace js::iv8::isolated;
	auto options = std::move(options_optional).value_or(make_agent_options{});
	auto& cluster = env.cluster();
	auto [ dispatch, promise ] = make_promise(env, [](environment& env, agent_handle agent, js::napi::unique_remote<function_tag> constructor) -> auto {
		auto class_template = js::napi::value<class_tag_of<agent_handle>>::from(constructor->deref(env));
		return js::forward{class_template.construct(env, std::move(agent))};
	});
	auto clock_ = std::visit(
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
			[](const make_agent_options::clock_system& /*options*/) -> clock::any_clock {
				return clock::system{};
			},
		},
		options.clock.value_or(make_agent_options::clock_system{})
	);
	agent_handle::make(
		cluster,
		[](const agent_handle::lock& lock) -> auto { return agent_environment{lock}; },
		{.clock = clock_, .random_seed = options.random_seed},
		[ dispatch = std::move(dispatch),
			constructor = js::napi::make_unique_remote(env, *constructor) ](
			const agent_handle::lock& /*lock*/,
			agent_handle agent
		) mutable -> void {
			dispatch(std::move(agent), std::move(constructor));
		}
	);

	return js::forward{promise};
}

auto agent_class_template(environment& env) -> js::napi::value<js::class_tag_of<agent_handle>> {
	return env.class_template(
		std::type_identity<agent_handle>{},
		js::class_template{
			js::class_constructor{util::cw<u8"Agent">},
			js::class_method{util::cw<u8"_compileModule">, make_forward_callback(module_handle::compile)},
			js::class_method{util::cw<u8"compileScript">, make_forward_callback(script_handle::compile_script)},
			js::class_method{util::cw<u8"createRealm">, make_forward_callback(realm_handle::create)},
			js::class_static{util::cw<u8"_create">, create_agent},
		}
	);
}

} // namespace backend_napi_v8

// Options visitors & acceptors
namespace js {
using backend_napi_v8::make_agent_options;

template <>
struct union_of<make_agent_options::clock_type> {
		constexpr static auto& discriminant = util::cw<"type">;
		constexpr static auto alternatives = std::tuple{
			alternative<make_agent_options::clock_deterministic>{"deterministic"},
			alternative<make_agent_options::clock_microtask>{"microtask"},
			alternative<make_agent_options::clock_realtime>{"realtime"},
			alternative<make_agent_options::clock_system>{"system"},
		};
};

} // namespace js
