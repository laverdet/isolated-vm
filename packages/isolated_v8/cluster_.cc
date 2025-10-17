export module isolated_v8:cluster;
import :foreground_runner;
import :platform;
import :scheduler;
import ivm.utility;
import v8;

namespace isolated_v8 {

// Owns a group of `agent` instances. There's one cluster per nodejs context (worker_thread).
// `cluster` is ultimately the owner of all agents it creates.
export class cluster : util::non_moveable {
	public:
		cluster() :
				platform_handle_{platform::handle::acquire()},
				scheduler_{platform_handle_->scheduler()} {}

		auto scheduler() -> scheduler::layer<{}>& { return scheduler_; }

	private:
		platform::handle platform_handle_;
		scheduler::layer<{}> scheduler_;
};

} // namespace isolated_v8
