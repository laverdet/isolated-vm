#include "environment.h"
#include <v8-platform.h>

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * ExecutorLock implementation
 */
thread_local IsolateEnvironment* IsolateEnvironment::ExecutorLock::current;
std::thread::id IsolateEnvironment::ExecutorLock::default_thread;

IsolateEnvironment::ExecutorLock::ExecutorLock(IsolateEnvironment& env) : last(current), locker(env.isolate), isolate_scope(env.isolate), handle_scope(env.isolate) {
	current = &env;
}

IsolateEnvironment::ExecutorLock::~ExecutorLock() {
	current = last;
}

void IsolateEnvironment::ExecutorLock::Init(IsolateEnvironment& default_isolate) {
	assert(current == nullptr);
	current = &default_isolate;
	default_thread = std::this_thread::get_id();
}

bool IsolateEnvironment::ExecutorLock::IsDefaultThread() {
	return std::this_thread::get_id() == default_thread;
}

/*
 * Scheduler implementation
 */
uv_async_t IsolateEnvironment::Scheduler::root_async;
thread_pool_t IsolateEnvironment::Scheduler::thread_pool(8);
std::atomic<int> IsolateEnvironment::Scheduler::uv_ref_count(0);

IsolateEnvironment::Scheduler::Scheduler() = default;
IsolateEnvironment::Scheduler::~Scheduler() = default;

void IsolateEnvironment::Scheduler::Init() {
	uv_async_init(uv_default_loop(), &root_async, AsyncCallbackRoot);
	root_async.data = nullptr;
	uv_unref((uv_handle_t*)&root_async);
}

void IsolateEnvironment::Scheduler::AsyncCallbackRoot(uv_async_t* async) {
	void* data = async->data;
	assert(data);
	async->data = nullptr;
	AsyncCallbackPool(true, data);
}

void IsolateEnvironment::Scheduler::AsyncCallbackPool(bool pool_thread, void* param) {
	auto isolate_ptr_ptr = (shared_ptr<IsolateEnvironment>*)param;
	auto isolate_ptr = shared_ptr<IsolateEnvironment>(std::move(*isolate_ptr_ptr));
	delete isolate_ptr_ptr;
	isolate_ptr->AsyncEntry();
	if (!pool_thread) {
		isolate_ptr->GetIsolate()->DiscardThreadSpecificMetadata();
	}
	if (--uv_ref_count == 0) {
		// Is there a possibility of a race condition here? What happens if uv_ref() below and this
		// execute at the same time. I don't think that's possible because if uv_ref_count is 0 that means
		// there aren't any concurrent threads running right now..
		uv_unref((uv_handle_t*)&root_async);
	}
}

IsolateEnvironment::Scheduler::Lock::Lock(Scheduler& scheduler) : scheduler(scheduler), lock(scheduler.mutex) {}
IsolateEnvironment::Scheduler::Lock::~Lock() = default;

void IsolateEnvironment::Scheduler::Lock::DoneRunning() {
	assert(scheduler.status == Status::Running);
	scheduler.status = Status::Waiting;
}

void IsolateEnvironment::Scheduler::Lock::PushTask(unique_ptr<Runnable> task) {
	scheduler.tasks.push(std::move(task));
}

void IsolateEnvironment::Scheduler::Lock::PushInterrupt(unique_ptr<std::function<void()>> interrupt) {
	scheduler.interrupts.push(std::move(interrupt));
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeTasks() {
	decltype(TakeTasks()) tmp;
	std::swap(tmp, scheduler.tasks);
	return tmp;
}

std::queue<unique_ptr<std::function<void()>>> IsolateEnvironment::Scheduler::Lock::TakeInterrupts() {
	decltype(TakeInterrupts()) tmp;
	std::swap(tmp, scheduler.interrupts);
	return tmp;
}

bool IsolateEnvironment::Scheduler::Lock::WakeIsolate(shared_ptr<IsolateEnvironment> isolate_ptr) {
	if (scheduler.status == Status::Waiting) {
		scheduler.status = Status::Running;
		IsolateEnvironment& isolate = *isolate_ptr;
		// Grab shared reference to this which will be passed to the worker entry. This ensures the
		// IsolateEnvironment won't be deleted before a thread picks up this work.
		auto isolate_ptr_ptr = new shared_ptr<IsolateEnvironment>(std::move(isolate_ptr));
		if (++uv_ref_count == 1) {
			uv_ref((uv_handle_t*)&root_async);
		}
		if (isolate.root) {
			assert(root_async.data == nullptr);
			root_async.data = isolate_ptr_ptr;
			uv_async_send(&root_async);
		} else {
			thread_pool.exec(scheduler.thread_affinity, Scheduler::AsyncCallbackPool, isolate_ptr_ptr);
		}
		return true;
	} else {
		return false;
	}
}

// Template specializations for IsolateSpecific<FunctionTemplate>
template<>
MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>();
}

template<>
void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle) {
	Reset<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>(handle);
}

// Static variable declarations
size_t IsolateEnvironment::specifics_count = 0;
shared_ptr<IsolateEnvironment::BookkeepingStatics> IsolateEnvironment::bookkeeping_statics_shared = std::make_shared<IsolateEnvironment::BookkeepingStatics>();

}
