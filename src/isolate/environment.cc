#include "environment.h"
#include "allocator.h"
#include "inspector.h"
#include "legacy.h"
#include "platform_delegate.h"
#include "runnable.h"
#include "../external_copy.h"
#include <cmath>

#ifdef USE_CLOCK_THREAD_CPUTIME_ID
#include <time.h>
#endif

#ifdef __APPLE__
#include <pthread.h>
static void* GetStackBase() {
	pthread_t self = pthread_self();
	return (void*)((char*)pthread_get_stackaddr_np(self) - pthread_get_stacksize_np(self));
}
#else
static void* GetStackBase() {
	return nullptr;
}
#endif

using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

namespace ivm {

/**
 * Executor implementation
 */
thread_local IsolateEnvironment* IsolateEnvironment::Executor::current_env = nullptr;
std::thread::id IsolateEnvironment::Executor::default_thread;
thread_local IsolateEnvironment::Executor::Lock* IsolateEnvironment::Executor::Lock::current = nullptr;
thread_local IsolateEnvironment::Executor::CpuTimer* IsolateEnvironment::Executor::cpu_timer_thread = nullptr;

IsolateEnvironment::Executor::Lock::Lock(
	IsolateEnvironment& env
) :
	last(current),
	scope(env),
	wall_timer(env.executor),
	locker(env.isolate),
	cpu_timer(env.executor),
	isolate_scope(env.isolate),
	handle_scope(env.isolate) {
	current = this;
}

IsolateEnvironment::Executor::Lock::~Lock() {
	current = last;
}

IsolateEnvironment::Executor::Unlock::Unlock(IsolateEnvironment& env) : pause_scope{env.executor.cpu_timer}, unlocker{env.isolate} {}

IsolateEnvironment::Executor::Unlock::~Unlock() = default;

IsolateEnvironment::Executor::CpuTimer::CpuTimer(Executor& executor) : executor{executor}, last{Executor::cpu_timer_thread}, time{Now()} {
	Executor::cpu_timer_thread = this;
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	assert(executor.cpu_timer == nullptr);
	executor.cpu_timer = this;
}

IsolateEnvironment::Executor::CpuTimer::~CpuTimer() {
	Executor::cpu_timer_thread = last;
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	executor.cpu_time += Now() - time;
	assert(executor.cpu_timer == this);
	executor.cpu_timer = nullptr;
}

std::chrono::nanoseconds IsolateEnvironment::Executor::CpuTimer::Delta(const std::lock_guard<std::mutex>& /* lock */) const {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(Now() - time);
}

void IsolateEnvironment::Executor::CpuTimer::Pause() {
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	executor.cpu_time += Now() - time;
	assert(executor.cpu_timer == this);
	executor.cpu_timer = nullptr;
}

void IsolateEnvironment::Executor::CpuTimer::Resume() {
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	time = Now();
	assert(executor.cpu_timer == nullptr);
	executor.cpu_timer = this;
}

#if USE_CLOCK_THREAD_CPUTIME_ID
IsolateEnvironment::Executor::CpuTimer::TimePoint IsolateEnvironment::Executor::CpuTimer::Now() {
	timespec ts;
	assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0);
	return TimePoint{std::chrono::duration_cast<std::chrono::system_clock::duration>(
		std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec)
	)};
}
#else
IsolateEnvironment::Executor::CpuTimer::TimePoint IsolateEnvironment::Executor::CpuTimer::Now() {
	return std::chrono::steady_clock::now();
}
#endif
IsolateEnvironment::Executor::CpuTimer::PauseScope::PauseScope(CpuTimer* timer) : timer(timer) {
	timer->Pause();
}

IsolateEnvironment::Executor::CpuTimer::PauseScope::~PauseScope() {
	timer->Resume();
}

IsolateEnvironment::Executor::CpuTimer::UnpauseScope::UnpauseScope(PauseScope& pause) : timer(pause.timer) {
	timer->Resume();
}

IsolateEnvironment::Executor::CpuTimer::UnpauseScope::~UnpauseScope() {
	timer->Pause();
}

IsolateEnvironment::Executor::WallTimer::WallTimer(Executor& executor) : executor(executor), cpu_timer(Executor::cpu_timer_thread) {
	// Pause current CPU timer which may not belong to this isolate
	if (cpu_timer != nullptr) {
		cpu_timer->Pause();
	}
	// Maybe start wall timer
	if (executor.wall_timer == nullptr) {
		std::lock_guard<std::mutex> lock(executor.timer_mutex);
		executor.wall_timer = this;
		time = std::chrono::steady_clock::now();
	}
}

IsolateEnvironment::Executor::WallTimer::~WallTimer() {
	// Resume old CPU timer
	if (cpu_timer != nullptr) {
		cpu_timer->Resume();
	}
	// Maybe update wall time
	if (executor.wall_timer == this) {
		std::lock_guard<std::mutex> lock(executor.timer_mutex);
		executor.wall_timer = nullptr;
		executor.wall_time += std::chrono::steady_clock::now() - time;
	}
}

std::chrono::nanoseconds IsolateEnvironment::Executor::WallTimer::Delta(const std::lock_guard<std::mutex>& /* lock */) const {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - time);
}

IsolateEnvironment::Executor::Executor(IsolateEnvironment& env) : env(env) {}

void IsolateEnvironment::Executor::Init(IsolateEnvironment& default_isolate) {
	assert(current_env == nullptr);
	current_env = &default_isolate;
	default_thread = std::this_thread::get_id();
}

bool IsolateEnvironment::Executor::IsDefaultThread() {
	return std::this_thread::get_id() == default_thread;
}

IsolateEnvironment::Executor::Scope::Scope(IsolateEnvironment& env) : last(current_env) {
	current_env = &env;
}

IsolateEnvironment::Executor::Scope::~Scope() {
	current_env = last;
}


/*
 * Scheduler implementation
 */
IsolateEnvironment::Scheduler* IsolateEnvironment::Scheduler::default_scheduler;
uv_async_t IsolateEnvironment::Scheduler::root_async;
thread_pool_t IsolateEnvironment::Scheduler::thread_pool(std::thread::hardware_concurrency() + 1);
std::atomic<unsigned int> IsolateEnvironment::Scheduler::uv_ref_count(0);

IsolateEnvironment::Scheduler::Scheduler() = default;
IsolateEnvironment::Scheduler::~Scheduler() = default;

void IsolateEnvironment::Scheduler::Init(IsolateEnvironment& default_isolate) {
	default_scheduler = &default_isolate.scheduler;
	uv_async_init(uv_default_loop(), &root_async, AsyncCallbackDefaultIsolate);
	root_async.data = nullptr;
	uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
}

void IsolateEnvironment::Scheduler::AsyncCallbackCommon(bool pool_thread, void* param) {
	auto isolate_ptr_ptr = static_cast<shared_ptr<IsolateEnvironment>*>(param);
	auto isolate_ptr = shared_ptr<IsolateEnvironment>(std::move(*isolate_ptr_ptr));
	delete isolate_ptr_ptr;
	isolate_ptr->AsyncEntry();
	if (!pool_thread) {
		isolate_ptr->GetIsolate()->DiscardThreadSpecificMetadata();
	}
}

void IsolateEnvironment::Scheduler::IncrementUvRef() {
	if (++uv_ref_count == 1) {
		// Only the default thread should be able to reach this branch
		assert(std::this_thread::get_id() == Executor::default_thread);
		uv_ref(reinterpret_cast<uv_handle_t*>(&root_async));
	}
}

void IsolateEnvironment::Scheduler::DecrementUvRef() {
	if (--uv_ref_count == 0) {
		if (std::this_thread::get_id() == Executor::default_thread) {
			uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		} else {
			uv_async_send(&root_async);
		}
	}
}

void IsolateEnvironment::Scheduler::AsyncCallbackNonDefaultIsolate(bool pool_thread, void* param) {
	AsyncCallbackCommon(pool_thread, param);
	if (--uv_ref_count == 0) {
		// Wake up the libuv loop so we can unref the async handle from the default thread.
		uv_async_send(&root_async);
	}
}

void IsolateEnvironment::Scheduler::AsyncCallbackDefaultIsolate(uv_async_t* async) {
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

void IsolateEnvironment::Scheduler::AsyncCallbackInterrupt(Isolate* /* isolate_ptr */, void* env_ptr) {
	IsolateEnvironment& env = *static_cast<IsolateEnvironment*>(env_ptr);
	env.InterruptEntry<&Lock::TakeInterrupts>();
}

void IsolateEnvironment::Scheduler::SyncCallbackInterrupt(Isolate* /* isolate_ptr */, void* env_ptr) {
	IsolateEnvironment& env = *static_cast<IsolateEnvironment*>(env_ptr);
	env.InterruptEntry<&Lock::TakeSyncInterrupts>();
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

void IsolateEnvironment::Scheduler::Lock::PushHandleTask(unique_ptr<Runnable> handle_task) {
	scheduler.handle_tasks.push(std::move(handle_task));
}

void IsolateEnvironment::Scheduler::Lock::PushInterrupt(unique_ptr<Runnable> interrupt) {
	scheduler.interrupts.push(std::move(interrupt));
}

void IsolateEnvironment::Scheduler::Lock::PushSyncInterrupt(unique_ptr<Runnable> interrupt) {
	scheduler.sync_interrupts.push(std::move(interrupt));
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeTasks() {
	decltype(scheduler.tasks) tmp;
	std::swap(tmp, scheduler.tasks);
	return tmp;
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeHandleTasks() {
	decltype(scheduler.handle_tasks) tmp;
	std::swap(tmp, scheduler.handle_tasks);
	return tmp;
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeInterrupts() {
	decltype(scheduler.interrupts) tmp;
	std::swap(tmp, scheduler.interrupts);
	return tmp;
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeSyncInterrupts() {
	decltype(scheduler.sync_interrupts) tmp;
	std::swap(tmp, scheduler.sync_interrupts);
	return tmp;
}

bool IsolateEnvironment::Scheduler::Lock::WakeIsolate(shared_ptr<IsolateEnvironment> isolate_ptr) {
	if (scheduler.status == Status::Waiting) {
		scheduler.status = Status::Running;
		IsolateEnvironment& isolate = *isolate_ptr;
		// Grab shared reference to this which will be passed to the worker entry. This ensures the
		// IsolateEnvironment won't be deleted before a thread picks up this work.
		auto isolate_ptr_ptr = new shared_ptr<IsolateEnvironment>(std::move(isolate_ptr));
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

void IsolateEnvironment::Scheduler::Lock::InterruptIsolate(IsolateEnvironment& isolate) {
	assert(scheduler.status == Status::Running);
	// Since this callback will be called by v8 we can be certain the pointer to `isolate` is still valid
	isolate->RequestInterrupt(AsyncCallbackInterrupt, static_cast<void*>(&isolate));
}

void IsolateEnvironment::Scheduler::Lock::InterruptSyncIsolate(IsolateEnvironment& isolate) {
	isolate->RequestInterrupt(SyncCallbackInterrupt, static_cast<void*>(&isolate));
}

IsolateEnvironment::Scheduler::AsyncWait::AsyncWait(Scheduler& scheduler) : scheduler(scheduler) {
	std::lock_guard<std::mutex> lock(scheduler.mutex);
	scheduler.async_wait = this;
}

IsolateEnvironment::Scheduler::AsyncWait::~AsyncWait() {
	std::lock_guard<std::mutex> lock(scheduler.mutex);
	scheduler.async_wait = nullptr;
}

void IsolateEnvironment::Scheduler::AsyncWait::Ready() {
	std::lock_guard<std::mutex> lock(scheduler.wait_mutex);
	ready = true;
	if (done) {
		scheduler.wait_cv.notify_one();
	}
}

void IsolateEnvironment::Scheduler::AsyncWait::Wait() {
	std::unique_lock<std::mutex> lock(scheduler.wait_mutex);
	while (!ready || !done) {
		scheduler.wait_cv.wait(lock);
	}
}

void IsolateEnvironment::Scheduler::AsyncWait::Wake() {
	std::lock_guard<std::mutex> lock(scheduler.wait_mutex);
	done = true;
	if (ready) {
		scheduler.wait_cv.notify_one();
	}
}

/**
 * HeapCheck implementation
 */
IsolateEnvironment::HeapCheck::HeapCheck(IsolateEnvironment& env, size_t expected_size) : env{env}, did_increase{false} {
	if (expected_size > 1024 && !env.root) {
		HeapStatistics heap;
		env.GetIsolate()->GetHeapStatistics(&heap);
#if V8_AT_LEAST(6, 7, 185)
		size_t old_space = env.memory_limit * 1024 * 1024;
#else
		size_t old_space = env.memory_limit * 1024 * 1024 * 2;
#endif
		size_t expected_total_heap = heap.used_heap_size() + env.extra_allocated_memory + expected_size;
		if (expected_total_heap > old_space) {
			// Heap limit increases by factor of 4
			if (expected_total_heap > old_space * 4) {
				throw js_generic_error("Value would likely exhaust isolate heap");
			}
			did_increase = true;
#if !V8_AT_LEAST(6, 7, 185)
			env.GetIsolate()->IncreaseHeapLimitForDebugging();
#endif
		}
	}
}

#if V8_AT_LEAST(6, 7, 185)
IsolateEnvironment::HeapCheck::~HeapCheck() = default;
#else

IsolateEnvironment::HeapCheck::~HeapCheck() {
	if (did_increase) {
		env.GetIsolate()->RestoreOriginalHeapLimit();
	}
}
#endif

void IsolateEnvironment::HeapCheck::Epilogue() {
	if (did_increase) {
		Isolate* isolate = env.GetIsolate();
		HeapStatistics heap;
		isolate->GetHeapStatistics(&heap);
		if (heap.used_heap_size() + env.extra_allocated_memory > env.memory_limit * 1024 * 1024) {
			isolate->LowMemoryNotification();
			isolate->GetHeapStatistics(&heap);
			if (heap.used_heap_size() + env.extra_allocated_memory > env.memory_limit * 1024 * 1024) {
				env.hit_memory_limit = true;
				env.Terminate();
				did_increase = false; // Don't reset heap limit to decrease chance v8 will OOM
				throw js_fatal_error("Isolate was disposed during execution due to memory limit");
			}
		}
	}
}

/**
 * IsolateEnvironment implementation
 */
size_t IsolateEnvironment::specifics_count = 0;
shared_ptr<IsolateEnvironment::BookkeepingStatics> IsolateEnvironment::bookkeeping_statics_shared = std::make_shared<IsolateEnvironment::BookkeepingStatics>(); // NOLINT

void IsolateEnvironment::GCEpilogueCallback(Isolate* isolate, GCType type, GCCallbackFlags /* flags */) {

	// Get current heap statistics
	auto that = GetCurrent();
	assert(that->isolate == isolate);
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);

	// If we are above the heap limit then kill this isolate
	if (type != GCType::kGCTypeIncrementalMarking && heap.used_heap_size() > that->memory_limit * 1024 * 1024) {
		that->hit_memory_limit = true;
		that->Terminate();
		return;
	}

	// Ask for a deep clean at 80% heap
	if (
		heap.used_heap_size() * 1.25 > that->memory_limit * 1024 * 1024 &&
		that->last_heap.used_heap_size() * 1.25 < that->memory_limit * 1024 * 1024
	) {
		struct LowMemoryTask : public Runnable {
			Isolate* isolate;
			explicit LowMemoryTask(Isolate* isolate) : isolate(isolate) {}
			void Run() final {
				isolate->LowMemoryNotification();
			}
		};
		Scheduler::Lock lock(that->scheduler);
		lock.PushHandleTask(std::make_unique<LowMemoryTask>(isolate));
	}

	that->last_heap = heap;
}

void IsolateEnvironment::OOMErrorCallback(const char* location, bool is_heap_oom) {
	fprintf(stderr, "%s\nis_heap_oom = %d\n\n\n", location, static_cast<int>(is_heap_oom));
	HeapStatistics heap;
	Isolate::GetCurrent()->GetHeapStatistics(&heap);
	fprintf(stderr,
		"<--- Heap statistics --->\n"
		"total_heap_size = %zd\n"
		"total_heap_size_executable = %zd\n"
		"total_physical_size = %zd\n"
		"total_available_size = %zd\n"
		"used_heap_size = %zd\n"
		"heap_size_limit = %zd\n"
		"malloced_memory = %zd\n"
		"peak_malloced_memory = %zd\n"
		"does_zap_garbage = %zd\n",
		heap.total_heap_size(),
		heap.total_heap_size_executable(),
		heap.total_physical_size(),
		heap.total_available_size(),
		heap.used_heap_size(),
		heap.heap_size_limit(),
		heap.malloced_memory(),
		heap.peak_malloced_memory(),
		heap.does_zap_garbage()
	);
	abort();
}

void IsolateEnvironment::PromiseRejectCallback(PromiseRejectMessage rejection) {
	auto that = IsolateEnvironment::GetCurrent();
	assert(that->isolate == Isolate::GetCurrent());
	that->rejected_promise_error.Reset(that->isolate, rejection.GetValue());
}

size_t IsolateEnvironment::NearHeapLimitCallback(void* data, size_t current_heap_limit, size_t /* initial_heap_limit */) {
	// This callback will temporarily give the v8 vm up to an extra 1 GB of memory to prevent the
	// application from crashing. After the JS stack unwinds the isolate will be disposed.
	auto that = static_cast<IsolateEnvironment*>(data);
	that->hit_memory_limit = true;
	that->Terminate();
	return current_heap_limit + 1024 * 1024 * 1024;
}

void IsolateEnvironment::AsyncEntry() {
	Executor::Lock lock(*this);
	if (!root) {
		// Set v8 stack limit on non-default isolate. This is only needed on non-default threads while
		// on OS X because it allocates just 512kb for each pthread stack, instead of 2mb on other
		// systems. 512kb is lower than the default v8 stack size so JS stack overflows result in
		// segfaults.
		thread_local void* stack_base = GetStackBase();
		if (stack_base != nullptr) {
			// Add 6kb of padding for native code to run
			isolate->SetStackLimit(reinterpret_cast<uintptr_t>(reinterpret_cast<char*>(stack_base) + 1024 * 6));
		}
	}

	while (true) {
		std::queue<unique_ptr<Runnable>> tasks;
		std::queue<unique_ptr<Runnable>> handle_tasks;
		std::queue<unique_ptr<Runnable>> interrupts;
		{
			// Grab current tasks
			Scheduler::Lock lock(scheduler);
			tasks = lock.TakeTasks();
			handle_tasks = lock.TakeHandleTasks();
			interrupts = lock.TakeInterrupts();
			if (tasks.empty() && handle_tasks.empty() && interrupts.empty()) {
				lock.DoneRunning();
				return;
			}
		}

		// Execute interrupt tasks
		while (!interrupts.empty()) {
			interrupts.front()->Run();
			interrupts.pop();
		}

		// Execute handle tasks
		while (!handle_tasks.empty()) {
			handle_tasks.front()->Run();
			handle_tasks.pop();
		}

		// Execute tasks
		while (!tasks.empty()) {
			tasks.front()->Run();
			tasks.pop();
			if (hit_memory_limit) {
				return;
			}
		}
	}
}

template <std::queue<std::unique_ptr<Runnable>> (IsolateEnvironment::Scheduler::Lock::*Take)()>
void IsolateEnvironment::InterruptEntry() {
	// Executor::Lock is already acquired
	while (true) {
		// Get interrupt callbacks
		std::queue<unique_ptr<Runnable>> interrupts;
		{
			Scheduler::Lock lock(scheduler);
			interrupts = (lock.*Take)();
			if (interrupts.empty()) {
				return;
			}
		}

		// Run the interrupts
		do {
			interrupts.front()->Run();
			interrupts.pop();
		} while (!interrupts.empty());
	}
}

IsolateEnvironment::IsolateEnvironment() :
	executor(*this),
	bookkeeping_statics(bookkeeping_statics_shared) {
}

void IsolateEnvironment::IsolateCtor(Isolate* isolate, Local<Context> context) {
	this->isolate = isolate;
	default_context.Reset(isolate, context);
	root = true;
	Executor::Init(*this);
	Scheduler::Init(*this);
	std::lock_guard<std::mutex> lock(bookkeeping_statics->lookup_mutex);
	bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
}

void IsolateEnvironment::IsolateCtor(size_t memory_limit, shared_ptr<void> snapshot_blob, size_t snapshot_length) {
	allocator_ptr = std::make_unique<LimitedAllocator>(*this, memory_limit * 1024 * 1024);
	snapshot_blob_ptr = std::move(snapshot_blob);
	this->memory_limit = memory_limit;
	root = false;

	// Calculate resource constraints
	ResourceConstraints rc;
#if defined(NODE_MODULE_VERSION) ? NODE_MODULE_VERSION >= 59 : V8_AT_LEAST(6, 1, 202)
	// Added in v8 bb29f9a4 but reverted in nodejs aa1a3ea9. It made it back in nodejs v10.x
	rc.set_max_semi_space_size_in_kb((int)std::pow(2, std::min(sizeof(void*) >= 8 ? 4.0 : 3.0, memory_limit / 128.0) + 10));
#else
	rc.set_max_semi_space_size((int)std::pow(2, std::min(sizeof(void*) >= 8 ? 4 : 3, (int)(memory_limit / 128))));
#endif

#if V8_AT_LEAST(6, 7, 185)
	rc.set_max_old_space_size((int)(memory_limit));
#else
	rc.set_max_old_space_size((int)(memory_limit * 2));
#endif

	// Build isolate from create params
	Isolate::CreateParams create_params;
	create_params.constraints = rc;
	create_params.array_buffer_allocator = allocator_ptr.get();
	if (snapshot_blob_ptr) {
		create_params.snapshot_blob = &startup_data;
		startup_data.data = reinterpret_cast<char*>(snapshot_blob_ptr.get());
		startup_data.raw_size = snapshot_length;
	}
	{
		PlatformDelegate::IsolateCtorScope scope(holder);
		isolate = Isolate::New(create_params);
	}
	{
		std::lock_guard<std::mutex> lock(bookkeeping_statics->lookup_mutex);
		bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
	}
	isolate->SetOOMErrorHandler(OOMErrorCallback);
	isolate->SetPromiseRejectCallback(PromiseRejectCallback);

#if V8_AT_LEAST(6, 7, 185)
	isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, static_cast<void*>(this));
#else
	isolate->AddGCEpilogueCallback(GCEpilogueCallback);
#endif

	// Create a default context for the library to use if needed
	{
		Locker locker(isolate);
		HandleScope handle_scope(isolate);
		default_context.Reset(isolate, NewContext());
	}

	// There is no asynchronous Isolate ctor so we should throw away thread specifics in case
	// the client always uses async methods
	isolate->DiscardThreadSpecificMetadata();
}

IsolateEnvironment::~IsolateEnvironment() {
	if (root) {
		return;
	}
	{
		// Grab local pointer to inspector agent with scheduler lock active
		std::unique_ptr<InspectorAgent> agent_ptr;
		{
			Scheduler::Lock lock(scheduler);
			agent_ptr = std::move(inspector_agent);
		}
		// Now activate executor lock and invoke inspector agent's dtor
		Executor::Lock lock(*this);
		agent_ptr.reset();
		// Kill all weak persistents
		for (auto it = weak_persistents.begin(); it != weak_persistents.end(); ) {
			void(*fn)(void*) = it->second.first;
			void* param = it->second.second;
			++it;
			fn(param);
		}
		assert(weak_persistents.empty());
		// Destroy outstanding tasks. Do this here while the executor lock is up.
		Scheduler::Lock lock2(scheduler);
		lock2.TakeInterrupts();
		lock2.TakeSyncInterrupts();
		lock2.TakeHandleTasks();
		lock2.TakeTasks();
	}
	{
		// Dispose() will call destructors for external strings and array buffers, so this lock sets the
		// "current" isolate for those C++ dtors to function correctly without locking v8
		Executor::Scope lock(*this);
		isolate->Dispose();
	}
	std::lock_guard<std::mutex> lock(bookkeeping_statics->lookup_mutex);
	bookkeeping_statics->isolate_map.erase(bookkeeping_statics->isolate_map.find(isolate));
}

static void DeserializeInternalFieldsCallback(Local<Object> /*holder*/, int /*index*/, StartupData /*payload*/, void* /*data*/) {
}

Local<Context> IsolateEnvironment::NewContext() {
#if NODE_MODULE_OR_V8_AT_LEAST(64, 6, 2, 193)
	return Context::New(isolate, nullptr, {}, {}, &DeserializeInternalFieldsCallback);
#else
	return Context::New(isolate);
#endif
}

void IsolateEnvironment::TaskEpilogue() {
	isolate->RunMicrotasks();
	if (hit_memory_limit) {
		throw js_fatal_error("Isolate was disposed during execution due to memory limit");
	}
	if (!rejected_promise_error.IsEmpty()) {
		Context::Scope context_scope(DefaultContext());
		isolate->ThrowException(Local<Value>::New(isolate, rejected_promise_error));
		rejected_promise_error.Reset();
		throw js_runtime_error();
	}
}

void IsolateEnvironment::EnableInspectorAgent() {
	inspector_agent = std::make_unique<InspectorAgent>(*this);
}

InspectorAgent* IsolateEnvironment::GetInspectorAgent() const {
	return inspector_agent.get();
}

std::chrono::nanoseconds IsolateEnvironment::GetCpuTime() {
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	std::chrono::nanoseconds time = executor.cpu_time;
	if (executor.cpu_timer != nullptr) {
		time += executor.cpu_timer->Delta(lock);
	}
	return time;
}

std::chrono::nanoseconds IsolateEnvironment::GetWallTime() {
	std::lock_guard<std::mutex> lock(executor.timer_mutex);
	std::chrono::nanoseconds time = executor.wall_time;
	if (executor.wall_timer != nullptr) {
		time += executor.wall_timer->Delta(lock);
	}
	return time;
}

void IsolateEnvironment::Terminate() {
	assert(!root);
	terminated = true;
	{
		Scheduler::Lock lock(scheduler);
		if (inspector_agent) {
			inspector_agent->Terminate();
		}
	}
	isolate->TerminateExecution();
	holder->isolate.reset();
}

void IsolateEnvironment::AddWeakCallback(Persistent<Object>* handle, void(*fn)(void*), void* param) {
	if (root) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it != weak_persistents.end()) {
		throw std::logic_error("Weak callback already added");
	}
	weak_persistents.insert(std::make_pair(handle, std::make_pair(fn, param)));
}

void IsolateEnvironment::RemoveWeakCallback(Persistent<Object>* handle) {
	if (root) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it == weak_persistents.end()) {
		throw std::logic_error("Weak callback doesn't exist");
	}
	weak_persistents.erase(it);
}

shared_ptr<IsolateHolder> IsolateEnvironment::LookupIsolate(Isolate* isolate) {
	std::lock_guard<std::mutex> lock(bookkeeping_statics_shared->lookup_mutex);
	auto it = bookkeeping_statics_shared->isolate_map.find(isolate);
	if (it == bookkeeping_statics_shared->isolate_map.end()) {
		return nullptr;
	} else {
		return it->second->holder;
	}
}

} // namespace ivm
