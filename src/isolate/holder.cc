#include "holder.h"
#include "environment.h"
#include "util.h"
#include <utility>

using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

void IsolateHolder::Dispose() {
	auto ref = std::exchange(state.write()->isolate, {});
	if (ref) {
		ref->Terminate();
		ref.reset();
	} else {
		throw js_generic_error("Isolate is already disposed");
	}
}

void IsolateHolder::ReleaseAndJoin() {
	auto ref = std::exchange(state.write()->isolate, {});
	ref.reset();
	auto lock = state.read();
	while (!lock->is_disposed) {
		cv.wait(lock);
	}
}

shared_ptr<IsolateEnvironment> IsolateHolder::GetIsolate() {
	return state.read()->isolate;
}

void IsolateHolder::ScheduleTask(unique_ptr<Runnable> task, bool run_inline, bool wake_isolate, bool handle_task) {
	auto ref = state.read()->isolate;
	if (ref) {
		if (run_inline && IsolateEnvironment::GetCurrent() == ref.get()) {
			task->Run();
			return;
		}
		Scheduler::Lock lock{ref->scheduler};
		if (handle_task) {
			lock.scheduler.handle_tasks.push(std::move(task));
		} else {
			lock.scheduler.tasks.push(std::move(task));
		}
		if (wake_isolate) {
			lock.scheduler.WakeIsolate(std::move(ref));
		}
	}
}

} // namespace ivm
