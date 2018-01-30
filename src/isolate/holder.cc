#include "holder.h"
#include "environment.h"
#include "util.h"

using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

IsolateHolder::IsolateHolder(shared_ptr<IsolateEnvironment> isolate) : isolate(std::move(isolate)) {}

void IsolateHolder::Dispose() {
	shared_ptr<IsolateEnvironment> tmp;
	std::swap(tmp, isolate);
	if (tmp) {
		tmp->Terminate();
		tmp.reset();
	} else {
		throw js_generic_error("Isolate is already disposed");
	}
}

shared_ptr<IsolateEnvironment> IsolateHolder::GetIsolate() {
	return isolate;
}

void IsolateHolder::ScheduleTask(unique_ptr<Runnable> task, bool run_inline, bool wake_isolate) {
	shared_ptr<IsolateEnvironment> ref = isolate;
	if (ref) {
		if (run_inline && IsolateEnvironment::GetCurrent() == ref.get()) {
			task->Run();
			return;
		}
		IsolateEnvironment::Scheduler::Lock lock(isolate->scheduler);
		lock.PushTask(std::move(task));
		if (wake_isolate) {
			lock.WakeIsolate(std::move(ref));
		}
	}
}

} // namespace ivm
