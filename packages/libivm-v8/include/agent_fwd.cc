module;
#include <concepts>
#include <memory>
export module ivm.isolated_v8:agent_fwd;
import :platform.foreground_runner;
import ivm.utility;

namespace ivm {

// The base `agent` class holds a weak reference to a `agent::host`. libivm directly controls the
// lifetime of a `host` and can sever the `weak_ptr` in this class if needed.
export class agent : util::non_copyable {
	public:
		class host;
		class lock;
		class managed_lock;
		class storage;

		explicit agent(
			const std::shared_ptr<host>& host,
			const std::shared_ptr<foreground_runner>& task_runner
		);

		auto schedule(std::invocable<lock&> auto task) -> void;

	private:
		std::weak_ptr<host> host_;
		std::shared_ptr<foreground_runner> task_runner_;
};

} // namespace ivm
