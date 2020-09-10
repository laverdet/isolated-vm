#include "holder.h"
#include "environment.h"
#include "scheduler.h"
#include "util.h"
#include "lib/timer.h"
#include <utility>

namespace ivm {

void IsolateDisposeWait::IsolateDidDispose() {
	*is_disposed.write() = true;
	is_disposed.notify_all();
}

void IsolateDisposeWait::Join() {
	auto lock = is_disposed.read<true>();
	while (!*lock) {
		lock.wait();
	}
}

auto IsolateHolder::Dispose() -> bool {
	auto ref = std::exchange(*isolate.write(), {});
	if (ref) {
		ref->Terminate();
		ref.reset();
		return true;
	} else {
		return false;
	}
}

void IsolateHolder::Release() {
	auto ref = std::exchange(*isolate.write(), {});
	ref.reset();
}

auto IsolateHolder::GetIsolate() -> std::shared_ptr<IsolateEnvironment> {
	return *isolate.read();
}

void IsolateHolder::ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate, bool handle_task) {
	auto ref = *isolate.read();
	if (ref) {
		if (run_inline && Executor::MayRunInlineTasks(*ref)) {
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

// Methods for v8::TaskRunner
void IsolateTaskRunner::PostTask(std::unique_ptr<v8::Task> task) {
	auto env = weak_env.lock();
	if (env) {
		Scheduler::Lock lock{env->GetScheduler()};
		lock.scheduler.tasks.push(std::move(task));
	}
}

void IsolateTaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) {
	// wait_detached erases the type of the lambda into a std::function which must be
	// copyable. The unique_ptr is stored in a shared pointer so ownership can be handled correctly.
	auto shared_task = std::make_shared<std::unique_ptr<v8::Task>>(std::move(task));
	auto weak_env = this->weak_env;
	timer_t::wait_detached(static_cast<uint32_t>(delay_in_seconds * 1000), [shared_task, weak_env](void* next) {
		auto env = weak_env.lock();
		if (env) {
			// Don't wake the isolate, this will just run the next time the isolate is doing something
			Scheduler::Lock lock{env->GetScheduler()};
			lock.scheduler.tasks.push(std::move(*shared_task));
		}
		timer_t::chain(next);
	});
}

} // namespace ivm
