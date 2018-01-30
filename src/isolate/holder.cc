#include "holder.h"
#include "environment.h"
#include "util.h"

using std::shared_ptr;

namespace ivm {

IsolateHolder::IsolateHolder(shared_ptr<IsolateEnvironment> isolate) : isolate(std::move(isolate)) {}

void IsolateHolder::Dispose() {
	shared_ptr<IsolateEnvironment> tmp;
	std::swap(tmp, isolate);
	if (tmp) {
		tmp->Dispose();
		tmp.reset();
	} else {
		throw js_generic_error("Isolate is already disposed");
		// TOOD: uh oh
	}
}

shared_ptr<IsolateEnvironment> IsolateHolder::GetIsolate() {
	return isolate;
}

void IsolateHolder::ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate) {
	shared_ptr<IsolateEnvironment> ref = isolate;
	if (ref) {
		ref->ScheduleTask(std::move(task), run_inline, wake_isolate);
	}
}

} // namespace ivm
