module;
#include <concepts>
#include <memory>
#include <optional>
#include <stop_token>
export module isolated_v8.agent:fwd;
import isolated_v8.clock;
import isolated_v8.cluster;
import ivm.utility;

namespace isolated_v8 {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;

		struct behavior_params {
				clock::any_clock clock;
				std::optional<double> random_seed;
		};

	private:
		class severable;

	public:
		agent(const std::shared_ptr<host>& host, std::shared_ptr<severable> severable_);

		auto schedule(auto task, auto... args) -> void
			requires std::invocable<decltype(task), lock&, decltype(args)...>;
		auto schedule_async(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const std::stop_token&, lock&, decltype(args)...>;

		static auto make(std::invocable<agent> auto fn, cluster& cluster, behavior_params params) -> void;

	private:
		std::weak_ptr<host> host_;
		std::shared_ptr<severable> severable_;
};

} // namespace isolated_v8
