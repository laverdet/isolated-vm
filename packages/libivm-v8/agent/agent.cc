module;
#include <functional>
#include <memory>
export module ivm.isolated_v8:agent.fwd;
import ivm.utility;

namespace ivm {

export class agent : non_copyable {
	public:
		class host;
		class lock;
		class storage;
		struct clock;
		using task_type = std::move_only_function<void(lock& lock)>;

		explicit agent(const std::shared_ptr<host>& host);
		auto schedule_task(task_type task) -> void;

	private:
		std::weak_ptr<host> host_;
};

agent::agent(const std::shared_ptr<host>& host) :
		host_{host} {}

} // namespace ivm
