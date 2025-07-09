module;
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
export module isolated_v8:agent_fwd;
import :agent_host_fwd;
import :clock;
import :cluster;

namespace isolated_v8 {

// The base `agent` class holds a weak reference to a `agent_host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent {
	public:
		class lock;

		struct behavior_params {
				clock::any_clock clock;
				std::optional<double> random_seed;
		};

		agent(const std::shared_ptr<agent_host>& host, std::shared_ptr<agent_severable> severable);

		auto schedule(auto task, auto... args) -> void
			requires std::invocable<decltype(task), lock&, decltype(args)...>;
		auto schedule_async(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const std::stop_token&, lock&, decltype(args)...>;

		static auto make(std::invocable<agent::lock&, agent> auto fn, cluster& cluster, behavior_params params) -> void;

	private:
		std::weak_ptr<agent_host> host_;
		std::shared_ptr<agent_severable> severable_;
};

} // namespace isolated_v8
