#include "environment.h"
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

/**
 * Template specializations for IsolateSpecific<FunctionTemplate>
 */
template<>
MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>();
}

template<>
void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle) {
	Reset<FunctionTemplate, decltype(IsolateEnvironment::specifics_ft), &IsolateEnvironment::specifics_ft>(handle);
}

/**
 * IsolateEnvironment implementation
 */
size_t IsolateEnvironment::specifics_count = 0;
shared_ptr<IsolateEnvironment::BookkeepingStatics> IsolateEnvironment::bookkeeping_statics_shared = std::make_shared<IsolateEnvironment::BookkeepingStatics>();

void IsolateEnvironment::GCEpilogueCallback(Isolate* isolate, GCType type, GCCallbackFlags flags) {

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
			LowMemoryTask(Isolate* isolate) : isolate(isolate) {}
			void Run() final {
				isolate->LowMemoryNotification();
			}
		};
		Scheduler::Lock lock(that->scheduler);
		lock.PushTask(std::make_unique<LowMemoryTask>(isolate));
	}

	that->last_heap = heap;
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
		std::queue<unique_ptr<std::function<void()>>> interrupts;
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
			(*interrupts.front())();
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
		startup_data.data = (const char*)snapshot_blob_ptr->Data();
		startup_data.raw_size = snapshot_blob_ptr->Length();
	}
	isolate = Isolate::New(create_params);
	{
		std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
		bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
	}
	isolate->AddGCEpilogueCallback(GCEpilogueCallback);
	isolate->SetPromiseRejectCallback(PromiseRejectCallback);

	// Bootstrap inspector
//			inspector_client = std::make_unique<InspectorClientImpl>(isolate, this);

	// Create a default context for the library to use if needed
	{
		Locker locker(isolate);
		HandleScope handle_scope(isolate);
		Local<Context> context = Context::New(isolate);
		default_context.Reset(isolate, context);
		std::string name = "default";
//			inspector_client->inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView((const uint8_t*)name.c_str(), name.length())));
	}

	// There is no asynchronous Isolate ctor so we should throw away thread specifics in case
	// the client always uses async methods
	isolate->DiscardThreadSpecificMetadata();
}

IsolateEnvironment::~IsolateEnvironment() {
	if (root) return;
	{
		ExecutorLock lock(*this);
		// Dispose of inspector first
//				inspector_client.reset();
		// Kill all weak persistents
		while (!weak_persistents.empty()) {
			auto it = weak_persistents.begin();
			Persistent<Object>* handle = it->first;
			void(*fn)(void*) = it->second.first;
			void* param = it->second.second;
			fn(param);
			if (weak_persistents.find(handle) != weak_persistents.end()) {
				// TODO: I can't throw from here
				throw std::runtime_error("Weak persistent callback failed to remove from global set");
			}
		}
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

HeapStatistics IsolateEnvironment::GetHeapStatistics() const {
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);
	return heap;
}

void IsolateEnvironment::AddWeakCallback(Persistent<Object>* handle, void(*fn)(void*), void* param) {
	if (root) return;
	auto it = weak_persistents.find(handle);
	if (it != weak_persistents.end()) {
		throw std::runtime_error("Weak callback already added");
	}
	weak_persistents.insert(std::make_pair(handle, std::make_pair(fn, param)));
}

void IsolateEnvironment::RemoveWeakCallback(Persistent<Object>* handle) {
	if (root) return;
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
