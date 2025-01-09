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
		cluster_scheduler scheduler_;
};

auto cluster::make_agent(std::invocable<agent> auto fn, clock::any_clock clock, std::optional<double> random_seed) -> void {
	auto agent_storage = std::make_shared<agent::storage>(scheduler_);
	auto& scheduler = agent_storage->scheduler();
	scheduler(
		[ clock, random_seed ](
			const std::stop_token& stop_token,
			std::shared_ptr<agent::storage> agent_storage,
			auto fn
		) {
			auto agent_host = std::make_shared<agent::host>(agent_storage, clock, random_seed);
			auto foreground_runner = agent_host->foreground_runner();
			auto agent = agent::host::make_handle(agent_host);
			agent_host.reset();
			std::invoke(std::move(fn), std::move(agent));
			foreground_runner->foreground_thread(stop_token);
			// nb: `agent_storage` contains the scheduler so it must be allowed to escape up the stack to
			// be released later.
			return agent_storage;
		},
		std::move(agent_storage),
		std::move(fn)
	);
};

} // namespace ivm
