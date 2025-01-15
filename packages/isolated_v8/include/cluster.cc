module;
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
export module ivm.isolated_v8:cluster;
import :agent;
import isolated_v8.clock;
import isolated_v8.foreground_runner;
import :platform;
import isolated_v8.scheduler;
import ivm.utility;
import v8;

namespace isolated_v8 {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : util::non_moveable {
	public:
		cluster();
		auto make_agent(std::invocable<agent> auto fn, clock::any_clock clock, std::optional<double> random_seed) -> void;

	private:
		platform::handle platform_handle_;
		scheduler::layer<{}> scheduler_;
};

auto cluster::make_agent(std::invocable<agent> auto fn, clock::any_clock clock, std::optional<double> random_seed) -> void {
	auto runner = std::make_shared<foreground_runner>(scheduler_);
	foreground_runner::schedule_client_task(
		runner,
		[ this, // I *think* this is safe because if `cluster` is destroyed the lambda won't be invoked
			clock,
			random_seed,
			runner,
			fn = std::move(fn) ](
			const std::stop_token& /*stop_token*/
		) mutable {
			auto agent_host = std::make_shared<agent::host>(scheduler_, runner, clock, random_seed);
			auto agent = agent::host::make_handle(agent_host);
			agent_host.reset();
			std::invoke(std::move(fn), std::move(agent));
			// nb: `runner` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return std::move(runner);
		}
	);
};

} // namespace isolated_v8
