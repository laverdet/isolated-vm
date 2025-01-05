module;
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
export module ivm.isolated_v8:cluster;
import :agent;
import :platform;
import :platform.clock;
import :platform.foreground_runner;
import :scheduler;
import ivm.utility;
import v8;

namespace ivm {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : util::non_moveable {
	public:
		cluster();
		auto make_agent(std::invocable<agent> auto fn, clock::any_clock clock, std::optional<double> random_seed) -> void;

	private:
		platform::handle platform_handle_;
		scheduler scheduler_;
};

auto cluster::make_agent(std::invocable<agent> auto fn, clock::any_clock clock, std::optional<double> random_seed) -> void {
	auto agent_storage = std::make_shared<agent::storage>(scheduler_);
	auto& handle = agent_storage->scheduler_handle();
	scheduler_.run(
		[ clock,
			random_seed,
			agent_storage = std::move(agent_storage),
			fn = std::move(fn) ](
			std::stop_token stop_token
		) mutable {
			auto task_runner = std::make_shared<foreground_runner>();
			auto agent_host = std::make_shared<agent::host>(agent_storage, task_runner, clock, random_seed);
			std::invoke(std::move(fn), agent{agent_host, task_runner});
			agent_host->execute(std::move(stop_token));
		},
		handle
	);
};

} // namespace ivm
