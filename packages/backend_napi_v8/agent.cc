module backend_napi_v8;
import :agent;
import :realm;
import :utility;
import std;
import util;

namespace backend_napi_v8 {

auto agent_handle_value::create(const environment::lock& lock, std::optional<create_options> options_optional) -> forward_promise_type {
	using namespace js::iv8::isolated;
	auto options = std::move(options_optional).value_or(create_options{});
	auto& cluster = lock->cluster();
	auto [ promise, resolver ] = make_promise(lock, [](const environment::lock& lock, agent_handle agent) -> auto {
		return js::forward{class_template(lock)->construct(lock, std::move(agent))};
	});
	auto memory_policy_ = [ & ] -> memory_policy::covariant {
		auto limit = options.memory_limit_bytes.value_or(0);
		if (limit == 0) {
			return {};
		} else {
			auto casted = static_cast<std::size_t>(limit);
			if (static_cast<double>(casted) != limit) {
				throw js::range_error("Invalid memoryLimitBytes: {}\n", limit);
			}
			return {std::type_identity<memory_policy::limited>{}, limit};
		}
	};
	auto clock_ = [ & ] -> clock::any_clock {
		auto clock_option = options.clock.value_or(clock_system{});
		return std::visit(
			util::overloaded{
				[](const clock_deterministic& options) -> clock::any_clock {
					return clock::deterministic{options.epoch, js::js_clock::duration{options.interval}};
				},
				[](const clock_microtask& options) -> clock::any_clock {
					return clock::microtask{options.epoch};
				},
				[](const clock_realtime& options) -> clock::any_clock {
					return clock::realtime{options.epoch};
				},
				[](const clock_system& /*options*/) -> clock::any_clock {
					return clock::system{};
				},
			},
			clock_option
		);
	};

	cluster.make_agent(
		[](const js::iv8::isolated::agent_handle::lock& lock) -> auto { return agent_environment{lock}; },
		{
			.memory_policy = memory_policy_(),
			.clock = clock_(),
			.random_seed = options.random_seed,
		},
		[ dispatch = std::move(resolver) ](
			const agent_handle::lock& /*lock*/,
			agent_handle agent
		) mutable -> void {
			dispatch(std::move(agent));
		}
	);

	return js::forward{promise};
}

auto agent_handle_value::create_realm(const environment::lock& lock) -> forward_promise_type {
	return realm_handle::create(lock, agent_);
}

auto agent_handle_value::compile_module(const environment::lock& lock, js::string_t source_text, compile_module_options options) -> forward_promise_type {
	return module_handle::compile(lock, agent_, std::move(source_text), std::move(options));
}

auto agent_handle_value::compile_script(const environment::lock& lock, js::string_t source_text, compile_script_options options) -> forward_promise_type {
	return script_handle::compile_script(lock, agent_, std::move(source_text), std::move(options));
}

auto agent_handle_value::dispose_async(const environment::lock& lock) -> forward_promise_type {
	if (disposed_) {
		return js::forward{disposed_.get(lock)};
	}
	auto [ promise, resolver ] = make_promise(lock);
	disposed_.reset(lock, promise);
	agent_.dispose([ resolver = std::move(resolver) ] mutable noexcept -> auto {
		resolver.resolve(std::monostate{});
	});
	return js::forward{promise};
}

auto agent_handle_value::class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<agent_handle_value>> {
	return lock->class_template(
		std::type_identity<agent_handle_value>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Agent">},
			js::class_method{util::cw<"compileModule">, util::fn<&agent_handle_value::compile_module>},
			js::class_method{util::cw<"compileScript">, util::fn<&agent_handle_value::compile_script>},
			js::class_method{util::cw<"createRealm">, util::fn<&agent_handle_value::create_realm>},
			js::class_method{util::cw<"disposeAsync">, util::fn<&agent_handle_value::dispose_async>},
			js::class_static{util::cw<"create">, &create},
		}
	);
}

} // namespace backend_napi_v8
