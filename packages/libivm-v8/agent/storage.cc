export module ivm.isolated_v8:agent.storage;
import :agent.fwd;
import :internal.scheduler;
import ivm.utility;

namespace ivm {

// This is constructed before (and may outlive) an agent
class agent::storage : non_moveable {
	public:
		explicit storage(scheduler& scheduler);

		auto scheduler_handle() -> scheduler::handle&;

	private:
		scheduler::handle scheduler_handle_;
};

agent::storage::storage(scheduler& scheduler) :
		scheduler_handle_{scheduler} {}

auto agent::storage::scheduler_handle() -> scheduler::handle& {
	return scheduler_handle_;
}

} // namespace ivm
