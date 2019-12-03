#include "environment.h"
#include "executor.h"
#include "scheduler.h"

using namespace v8;
namespace ivm {

/*
 * Scheduler implementation
 */
Scheduler* Scheduler::default_scheduler;
uv_async_t Scheduler::root_async;
thread_pool_t Scheduler::thread_pool(std::thread::hardware_concurrency() + 1);
std::atomic<unsigned int> Scheduler::uv_ref_count(0);

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

void Scheduler::Init(IsolateEnvironment& default_isolate) {
	default_scheduler = &default_isolate.scheduler;
	uv_async_init(uv_default_loop(), &root_async, AsyncCallbackDefaultIsolate);
	root_async.data = nullptr;
	uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
}

void Scheduler::AsyncCallbackCommon(bool pool_thread, void* param) {
	auto isolate_ptr_ptr = static_cast<std::shared_ptr<IsolateEnvironment>*>(param);
	auto isolate_ptr = std::shared_ptr<IsolateEnvironment>(std::move(*isolate_ptr_ptr));
	delete isolate_ptr_ptr;
	isolate_ptr->AsyncEntry();
	if (!pool_thread) {
		isolate_ptr->GetIsolate()->DiscardThreadSpecificMetadata();
	}
}

void Scheduler::IncrementUvRef() {
	if (++uv_ref_count == 1) {
		// Only the default thread should be able to reach this branch
		assert(std::this_thread::get_id() == Executor::default_thread);
		uv_ref(reinterpret_cast<uv_handle_t*>(&root_async));
	}
}

void Scheduler::DecrementUvRef() {
	if (--uv_ref_count == 0) {
		if (std::this_thread::get_id() == Executor::default_thread) {
			uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		} else {
			uv_async_send(&root_async);
		}
	}
}

void Scheduler::AsyncCallbackNonDefaultIsolate(bool pool_thread, void* param) {
	AsyncCallbackCommon(pool_thread, param);
	if (--uv_ref_count == 0) {
		// Wake up the libuv loop so we can unref the async handle from the default thread.
		uv_async_send(&root_async);
	}
}

void Scheduler::AsyncCallbackDefaultIsolate(uv_async_t* async) {
	// We need a lock on the default isolate's scheduler to access this data because it can be
	// modified from `WakeIsolate` while `AsyncCallbackNonDefaultIsolate` (which doesn't acquire a
	// lock) is triggering the uv_async_send.
	void* data;
	{
		Lock scheduler_lock(*default_scheduler);
		data = async->data;
		async->data = nullptr;
	}
	if (data == nullptr) {
		if (uv_ref_count.load() == 0) {
			uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		}
	} else {
		AsyncCallbackCommon(true, data);
		if (--uv_ref_count == 0) {
			uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		}
	}
}

void Scheduler::AsyncCallbackInterrupt(Isolate* /* isolate_ptr */, void* env_ptr) {
	IsolateEnvironment& env = *static_cast<IsolateEnvironment*>(env_ptr);
	env.InterruptEntry<&Lock::TakeInterrupts>();
}

void Scheduler::SyncCallbackInterrupt(Isolate* /* isolate_ptr */, void* env_ptr) {
	IsolateEnvironment& env = *static_cast<IsolateEnvironment*>(env_ptr);
	env.InterruptEntry<&Lock::TakeSyncInterrupts>();
}

Scheduler::Lock::Lock(Scheduler& scheduler) : scheduler(scheduler), lock(scheduler.mutex) {}
Scheduler::Lock::~Lock() = default;

void Scheduler::Lock::DoneRunning() {
	assert(scheduler.status == Status::Running);
	scheduler.status = Status::Waiting;
}

void Scheduler::Lock::PushTask(std::unique_ptr<Runnable> task) {
	scheduler.tasks.push(std::move(task));
}

void Scheduler::Lock::PushHandleTask(std::unique_ptr<Runnable> handle_task) {
	scheduler.handle_tasks.push(std::move(handle_task));
}

void Scheduler::Lock::PushInterrupt(std::unique_ptr<Runnable> interrupt) {
	scheduler.interrupts.push(std::move(interrupt));
}

void Scheduler::Lock::PushSyncInterrupt(std::unique_ptr<Runnable> interrupt) {
	scheduler.sync_interrupts.push(std::move(interrupt));
}

std::queue<std::unique_ptr<Runnable>> Scheduler::Lock::TakeTasks() {
	decltype(scheduler.tasks) tmp;
	std::swap(tmp, scheduler.tasks);
	return tmp;
}

std::queue<std::unique_ptr<Runnable>> Scheduler::Lock::TakeHandleTasks() {
	decltype(scheduler.handle_tasks) tmp;
	std::swap(tmp, scheduler.handle_tasks);
	return tmp;
}

std::queue<std::unique_ptr<Runnable>> Scheduler::Lock::TakeInterrupts() {
	decltype(scheduler.interrupts) tmp;
	std::swap(tmp, scheduler.interrupts);
	return tmp;
}

std::queue<std::unique_ptr<Runnable>> Scheduler::Lock::TakeSyncInterrupts() {
	decltype(scheduler.sync_interrupts) tmp;
	std::swap(tmp, scheduler.sync_interrupts);
	return tmp;
}

bool Scheduler::Lock::WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate_ptr) {
	if (scheduler.status == Status::Waiting) {
		scheduler.status = Status::Running;
		IsolateEnvironment& isolate = *isolate_ptr;
		// Grab shared reference to this which will be passed to the worker entry. This ensures the
		// IsolateEnvironment won't be deleted before a thread picks up this work.
		auto isolate_ptr_ptr = new std::shared_ptr<IsolateEnvironment>(std::move(isolate_ptr));
		IncrementUvRef();
		if (isolate.root) {
			assert(root_async.data == nullptr);
			root_async.data = isolate_ptr_ptr;
			uv_async_send(&root_async);
		} else {
			thread_pool.exec(scheduler.thread_affinity, Scheduler::AsyncCallbackNonDefaultIsolate, isolate_ptr_ptr);
		}
		return true;
	} else {
		return false;
	}
}

void Scheduler::Lock::InterruptIsolate(IsolateEnvironment& isolate) {
	assert(scheduler.status == Status::Running);
	// Since this callback will be called by v8 we can be certain the pointer to `isolate` is still valid
	isolate->RequestInterrupt(AsyncCallbackInterrupt, static_cast<void*>(&isolate));
}

void Scheduler::Lock::InterruptSyncIsolate(IsolateEnvironment& isolate) {
	isolate->RequestInterrupt(SyncCallbackInterrupt, static_cast<void*>(&isolate));
}

Scheduler::AsyncWait::AsyncWait(Scheduler& scheduler) : scheduler(scheduler) {
	std::lock_guard<std::mutex> lock(scheduler.mutex);
	scheduler.async_wait = this;
}

Scheduler::AsyncWait::~AsyncWait() {
	std::lock_guard<std::mutex> lock(scheduler.mutex);
	scheduler.async_wait = nullptr;
}

void Scheduler::AsyncWait::Ready() {
	std::lock_guard<std::mutex> lock(scheduler.wait_mutex);
	ready = true;
	if (done) {
		scheduler.wait_cv.notify_one();
	}
}

void Scheduler::AsyncWait::Wait() {
	std::unique_lock<std::mutex> lock(scheduler.wait_mutex);
	while (!ready || !done) {
		scheduler.wait_cv.wait(lock);
	}
}

void Scheduler::AsyncWait::Wake() {
	std::lock_guard<std::mutex> lock(scheduler.wait_mutex);
	done = true;
	if (ready) {
		scheduler.wait_cv.notify_one();
	}
}

} // namespace ivm
