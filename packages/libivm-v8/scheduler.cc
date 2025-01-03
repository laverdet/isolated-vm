module;
#include <boost/intrusive/list.hpp>
#include <stop_token>
module ivm.isolated_v8;
import :scheduler;
import ivm.utility;

namespace ivm {

scheduler::~scheduler() {
	// Send stop signal to all handles and wait for `handles` to drain before continuing with
	// destruction.
	auto lock = handles.read_waitable([](const handle_list& handles) {
		return handles.empty();
	});
	for (const auto& handle : *lock) {
		handle.stop_source_.write()->request_stop();
	}
	lock.wait();
}

scheduler::handle::handle(scheduler& scheduler) :
		scheduler_{scheduler},
		stop_source_{std::nostopstate} {
	scheduler_.handles.write()->push_back(*this);
}

scheduler::handle::~handle() {
	auto lock = scheduler_.handles.write_notify([](const handle_list& handles) {
		return handles.empty();
	});
	scheduler_hook_.unlink();
}

} // namespace ivm
