#include "holder.h"
#include "environment.h"
#include "util.h"
#include <utility>

using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

IsolateHolder::IsolateHolder(shared_ptr<IsolateEnvironment> isolate) : isolate(std::move(isolate)) {}

void IsolateHolder::Dispose() {
	shared_ptr<IsolateEnvironment> tmp;
	{
		std::lock_guard<std::mutex> lock{mutex};
		tmp = std::exchange(isolate, {});
	}
	if (tmp) {
		tmp->Terminate();
		tmp.reset();
	} else {
		throw js_generic_error("Isolate is already disposed");
	}
}

shared_ptr<IsolateEnvironment> IsolateHolder::GetIsolate() {
	std::lock_guard<std::mutex> lock{mutex};
	return isolate;
}

void IsolateHolder::ScheduleTask(unique_ptr<Runnable> task, bool run_inline, bool wake_isolate, bool handle_task) {
	shared_ptr<IsolateEnvironment> ref;
	{
		std::lock_guard<std::mutex> lock{mutex};
		ref = isolate;
	}
	if (ref) {
		if (run_inline && IsolateEnvironment::GetCurrent() == ref.get()) {
			task->Run();
			return;
		}
		IsolateEnvironment::Scheduler::Lock lock(ref->scheduler);
		if (handle_task) {
			lock.PushHandleTask(std::move(task));
		} else {
			lock.PushTask(std::move(task));
		}
		if (wake_isolate) {
			lock.WakeIsolate(std::move(ref));
		}
	}
}

} // namespace ivm
