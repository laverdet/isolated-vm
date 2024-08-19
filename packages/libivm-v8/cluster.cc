module;
#include <concepts>
#include <memory>
#include <stop_token>
#include <utility>
export module ivm.isolated_v8:cluster;
import :agent;
import :agent.storage;
import :internal.platform_handle;
import :internal.scheduler;
import ivm.utility;

namespace ivm {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : non_moveable {
	public:
		cluster();
		auto make_agent(std::invocable<agent, agent::lock&> auto fn) -> void;

	private:
		platform_handle platform_handle_;
		scheduler scheduler_;
};

cluster::cluster() :
		platform_handle_{platform_handle::acquire()} {
}

auto cluster::make_agent(std::invocable<agent, agent::lock&> auto fn) -> void {
	auto agent_storage = std::make_shared<agent::storage>(scheduler_);
	auto* storage_ptr = agent_storage.get();
	scheduler_.run(
		[](const std::stop_token& stop_token, std::shared_ptr<agent::storage> agent_storage, auto fn) {
			auto agent_host = std::make_shared<agent::host>(std::move(agent_storage));
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
