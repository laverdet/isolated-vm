module;
#include <concepts>
#include <memory>
#include <stop_token>
export module ivm.isolated_v8:agent_fwd;
import isolated_v8.foreground_runner;
import ivm.utility;

namespace isolated_v8 {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;

	private:
		class severable;

	public:
		agent(const std::shared_ptr<host>& host, std::shared_ptr<severable> severable_);

		auto schedule(auto task, auto... args) -> void
			requires std::invocable<decltype(task), lock&, decltype(args)...>;
		auto schedule_async(auto task, auto... args) -> void
			requires std::invocable<decltype(task), const std::stop_token&, lock&, decltype(args)...>;

		std::weak_ptr<host> host_;
		std::shared_ptr<severable> severable_;
};

} // namespace isolated_v8
