module;
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
export module ivm.isolated_v8:cluster;
import :agent;
import :platform;
import :scheduler;

namespace ivm {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : non_moveable {
	public:
		cluster();
		auto make_agent(
			std::invocable<agent, agent::lock&> auto fn,
			agent::clock::any_clock clock,
			std::optional<double> random_seed
		) -> void;

	private:
		platform::handle platform_handle_;
		scheduler scheduler_;
};

auto cluster::make_agent(std::invocable<agent, agent::lock&> auto fn, agent::clock::any_clock clock, std::optional<double> random_seed) -> void {
	auto agent_storage = std::make_shared<agent::storage>(scheduler_);
	auto* storage_ptr = agent_storage.get();
	scheduler_.run(
		[ clock, random_seed ](
			const std::stop_token& stop_token,
			std::shared_ptr<agent::storage> agent_storage,
			auto fn
		) {
			auto agent_host = std::make_shared<agent::host>(std::move(agent_storage), clock, random_seed);
			agent::lock agent_lock{*agent_host};
			take(std::move(fn))(agent{agent_host}, agent_lock);
			agent_host->execute(stop_token);
		},
		storage_ptr->scheduler_handle(),
		std::move(agent_storage),
		std::move(fn)
	);
};

} // namespace ivm
