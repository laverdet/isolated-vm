#include "environment.h"
#include "inspector.h"
#include "runnable.h"
#include "../external_copy.h"
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
	uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
}

void IsolateEnvironment::Scheduler::AsyncCallbackRoot(uv_async_t* async) {
	if (async->data == nullptr) {
		// This is the final message
		return;
	}
	void* data = async->data;
	async->data = nullptr;
	AsyncCallbackPool(true, data);
}

void IsolateEnvironment::Scheduler::AsyncCallbackPool(bool pool_thread, void* param) {
	{
		auto isolate_ptr_ptr = static_cast<shared_ptr<IsolateEnvironment>*>(param);
		auto isolate_ptr = shared_ptr<IsolateEnvironment>(std::move(*isolate_ptr_ptr));
		delete isolate_ptr_ptr;
		isolate_ptr->AsyncEntry();
		if (!pool_thread) {
			isolate_ptr->GetIsolate()->DiscardThreadSpecificMetadata();
		}
	}
	// nb: We reset the isolate pointer right now with the scoped block above. It's possible that this
	// is the last remaining reference to the isolate, in which case the dtor for IsolateEnvironment
	// will be called. If the environment has pending tasks these will be disposed without running. It
	// is then further possible that those disposed tasks will schedule more work from their dtors
	// (for instance calling Promise#reject). This all needs to happen *before* the uv_unref stuff
	// below because otherwise we can end up in a situation where node thinks all work is done but
	// isolated-vm is still finishing up.
	if (--uv_ref_count == 0) {
		// Is there a possibility of a race condition here? What happens if uv_ref() below and this
		// execute at the same time. I don't think that's possible because if uv_ref_count is 0 that means
		// there aren't any concurrent threads running right now..
		uv_unref(reinterpret_cast<uv_handle_t*>(&root_async));
		// For some reason sending a pointless ping to the unref'd uv handle allows node to quit when
		// isolated-vm is done.
		assert(root_async.data == nullptr);
		uv_async_send(&root_async);
	}
}


void IsolateEnvironment::Scheduler::AsyncCallbackInterrupt(Isolate* /* isolate_ptr */, void* env_ptr) {
	IsolateEnvironment& env = *static_cast<IsolateEnvironment*>(env_ptr);
	env.InterruptEntry();
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

void IsolateEnvironment::Scheduler::Lock::PushInterrupt(unique_ptr<Runnable> interrupt) {
	scheduler.interrupts.push(std::move(interrupt));
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeTasks() {
	decltype(scheduler.tasks) tmp;
	std::swap(tmp, scheduler.tasks);
	return tmp;
}

std::queue<unique_ptr<Runnable>> IsolateEnvironment::Scheduler::Lock::TakeInterrupts() {
	decltype(scheduler.interrupts) tmp;
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
			uv_ref(reinterpret_cast<uv_handle_t*>(&root_async));
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

void IsolateEnvironment::Scheduler::Lock::InterruptIsolate(IsolateEnvironment& isolate) {
	assert(scheduler.status == Status::Running);
	// Since this callback will be called by v8 we can be certain the pointer to `isolate` is still valid
	isolate->RequestInterrupt(AsyncCallbackInterrupt, static_cast<void*>(&isolate));
}

/**
 * HeapCheck implementation
 */
IsolateEnvironment::HeapCheck::HeapCheck(IsolateEnvironment& env, size_t expected_size) : env(env), did_increase(false) {
	if (expected_size > 1024 && !env.root) {
		HeapStatistics heap;
		env.GetIsolate()->GetHeapStatistics(&heap);
		size_t old_space = env.memory_limit * 1024 * 1024 * 2;
		size_t expected_total_heap = heap.used_heap_size() + expected_size;
		if (expected_total_heap > old_space) {
			// Heap limit increases by factor of 4
			if (expected_total_heap > old_space * 4) {
				throw js_generic_error("Value would likely exhaust isolate heap");
			}
			did_increase = true;
			env.GetIsolate()->IncreaseHeapLimitForDebugging();
		}
	}
}

IsolateEnvironment::HeapCheck::~HeapCheck() {
	if (did_increase) {
		env.GetIsolate()->RestoreOriginalHeapLimit();
	}
}

void IsolateEnvironment::HeapCheck::Epilogue() {
	if (did_increase) {
		HeapStatistics heap;
		env.GetIsolate()->GetHeapStatistics(&heap);
		if (heap.used_heap_size() > env.memory_limit * 1024 * 1024) {
			env.hit_memory_limit = true;
			env.Terminate();
			did_increase = false; // Don't reset heap limit to decrease chance v8 will OOM
			throw js_fatal_error();
		}
	}
}

/**
 * Template specializations for IsolateSpecific<FunctionTemplate>
 */
template<>
MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>();
}

template<>
void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Set(Local<FunctionTemplate> handle) {
	Set<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>(handle);
}

/**
 * IsolateEnvironment implementation
 */
size_t IsolateEnvironment::specifics_count = 0;
shared_ptr<IsolateEnvironment::BookkeepingStatics> IsolateEnvironment::bookkeeping_statics_shared = std::make_shared<IsolateEnvironment::BookkeepingStatics>(); // NOLINT

void IsolateEnvironment::GCEpilogueCallback(Isolate* isolate, GCType /* type */, GCCallbackFlags /* flags */) {

	// Get current heap statistics
	auto that = GetCurrent();
	assert(that->isolate == isolate);
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);

	// If we are above the heap limit then kill this isolate
	if (heap.used_heap_size() > that->memory_limit * 1024 * 1024) {
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
		lock.PushTask(std::make_unique<LowMemoryTask>(isolate));
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

void IsolateEnvironment::AsyncEntry() {
	ExecutorLock lock(*this);
	while (true) {
		std::queue<unique_ptr<Runnable>> tasks;
		std::queue<unique_ptr<Runnable>> interrupts;
		{
			// Grab current tasks
			Scheduler::Lock lock(scheduler);
			tasks = lock.TakeTasks();
			interrupts = lock.TakeInterrupts();
			if (tasks.empty() && interrupts.empty()) {
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
		while (!tasks.empty()) {
			tasks.front()->Run();
			tasks.pop();
			if (hit_memory_limit) {
				return;
			}
		}
	}
}

void IsolateEnvironment::InterruptEntry() {
	// ExecutorLock is already acquired
	while (true) {
		// Get interrupt callbacks
		std::queue<unique_ptr<Runnable>> interrupts;
		{
			Scheduler::Lock lock(scheduler);
			interrupts = lock.TakeInterrupts();
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

IsolateEnvironment::IsolateEnvironment(Isolate* isolate, Local<Context> context) :
	isolate(isolate),
	default_context(isolate, context),
	root(true),
	bookkeeping_statics(bookkeeping_statics_shared) {
	ExecutorLock::Init(*this);
	Scheduler::Init();
	std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
	bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
}

IsolateEnvironment::IsolateEnvironment(
	const ResourceConstraints& resource_constraints,
	unique_ptr<ArrayBuffer::Allocator> allocator,
	shared_ptr<ExternalCopyArrayBuffer> snapshot_blob,
	size_t memory_limit
) :
	allocator_ptr(std::move(allocator)),
	snapshot_blob_ptr(std::move(snapshot_blob)),
	memory_limit(memory_limit),
	root(false),
	bookkeeping_statics(bookkeeping_statics_shared) {

	// Build isolate from create params
	Isolate::CreateParams create_params;
	create_params.constraints = resource_constraints;
	create_params.array_buffer_allocator = allocator_ptr.get();
	if (snapshot_blob_ptr) {
		create_params.snapshot_blob = &startup_data;
		startup_data.data = reinterpret_cast<const char*>(snapshot_blob_ptr->Data());
		startup_data.raw_size = snapshot_blob_ptr->Length();
	}
	isolate = Isolate::New(create_params);
	{
		std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
		bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
	}
	isolate->AddGCEpilogueCallback(GCEpilogueCallback);
	isolate->SetOOMErrorHandler(OOMErrorCallback);
	isolate->SetPromiseRejectCallback(PromiseRejectCallback);

	// Create a default context for the library to use if needed
	{
		Locker locker(isolate);
		HandleScope handle_scope(isolate);
		default_context.Reset(isolate, Context::New(isolate));
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
		ExecutorLock lock(*this);
		// Dispose of inspector first
		inspector_agent.reset();
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
		lock2.TakeTasks();
	}
	isolate->Dispose();
	std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
	bookkeeping_statics->isolate_map.erase(bookkeeping_statics->isolate_map.find(isolate));
}

void IsolateEnvironment::TaskEpilogue() {
	isolate->RunMicrotasks();
	if (hit_memory_limit) {
		throw js_fatal_error();
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

void IsolateEnvironment::AddWeakCallback(Persistent<Object>* handle, void(*fn)(void*), void* param) {
	if (root) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it != weak_persistents.end()) {
		throw std::runtime_error("Weak callback already added");
	}
	weak_persistents.insert(std::make_pair(handle, std::make_pair(fn, param)));
}

void IsolateEnvironment::RemoveWeakCallback(Persistent<Object>* handle) {
	if (root) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it == weak_persistents.end()) {
		throw std::runtime_error("Weak callback doesn't exist");
	}
	weak_persistents.erase(it);
}

shared_ptr<IsolateHolder> IsolateEnvironment::LookupIsolate(Isolate* isolate) {
	std::unique_lock<std::mutex> lock(bookkeeping_statics_shared->lookup_mutex);
	auto it = bookkeeping_statics_shared->isolate_map.find(isolate);
	if (it == bookkeeping_statics_shared->isolate_map.end()) {
		return nullptr;
	} else {
		return it->second->holder;
	}
}

} // namespace ivm
