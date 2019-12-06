#include "environment.h"
#include "allocator.h"
#include "inspector.h"
#include "platform_delegate.h"
#include "runnable.h"
#include "../external_copy.h"
#include "scheduler.h"
#include <algorithm>
#include <cmath>
#include <mutex>

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
 * HeapCheck implementation
 */
IsolateEnvironment::HeapCheck::HeapCheck(IsolateEnvironment& env, bool force) :
		env{env}, extra_size_before{env.extra_allocated_memory}, force{force} {
}

void IsolateEnvironment::HeapCheck::Epilogue() {
	if (!env.nodejs_isolate && (force || env.extra_allocated_memory != extra_size_before)) {
		Isolate* isolate = env.GetIsolate();
		HeapStatistics heap;
		isolate->GetHeapStatistics(&heap);
		if (heap.used_heap_size() + env.extra_allocated_memory > env.memory_limit) {
			isolate->LowMemoryNotification();
			isolate->GetHeapStatistics(&heap);
			if (heap.used_heap_size() + env.extra_allocated_memory > env.memory_limit) {
				env.hit_memory_limit = true;
				env.Terminate();
				throw js_fatal_error("Isolate was disposed during execution due to memory limit");
			}
		}
	}
}

/**
 * IsolateEnvironment implementation
 */
size_t IsolateEnvironment::specifics_count = 0;
std::shared_ptr<IsolateEnvironment::IsolateMap> IsolateEnvironment::isolate_map_shared = std::make_shared<IsolateMap>();

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

void IsolateEnvironment::MarkSweepCompactEpilogue(Isolate* isolate, GCType gc_type, GCCallbackFlags gc_flags, void* data) {
	auto that = static_cast<IsolateEnvironment*>(data);
	HeapStatistics heap;
	that->isolate->GetHeapStatistics(&heap);
	size_t total_memory = heap.used_heap_size() + that->extra_allocated_memory;
	size_t memory_limit = that->memory_limit + that->misc_memory_size;
	if (total_memory > memory_limit) {
		if (gc_flags & (GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage | GCCallbackFlags::kGCCallbackFlagForced)) {
			that->Terminate();
			that->hit_memory_limit = true;
		} else {
			// Force full garbage collection
			that->RequestMemoryPressureNotification(MemoryPressureLevel::kCritical, true);
		}
	} else if (!(gc_flags & GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage)) {
		if (that->did_adjust_heap_limit) {
			// There is also `AutomaticallyRestoreInitialHeapLimit` introduced in v8 7.3.411 / 93283bf04
			// but it seems less effective than this ratcheting strategy.
			isolate->RemoveNearHeapLimitCallback(NearHeapLimitCallback, that->memory_limit);
			isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, data);
			HeapStatistics heap;
			that->isolate->GetHeapStatistics(&heap);
			if (heap.heap_size_limit() == that->initial_heap_size_limit) {
				that->did_adjust_heap_limit = false;
			}
		}
		if (total_memory + total_memory / 4 > memory_limit) {
			// Send "moderate" pressure at 80%
			that->RequestMemoryPressureNotification(MemoryPressureLevel::kModerate, true);
		}
	}
}

size_t IsolateEnvironment::NearHeapLimitCallback(void* data, size_t current_heap_limit, size_t /* initial_heap_limit */) {
	// This callback will temporarily give the v8 vm up to an extra 1 GB of memory to prevent the
	// application from crashing.
	auto that = static_cast<IsolateEnvironment*>(data);
	that->did_adjust_heap_limit = true;
	HeapStatistics heap;
	that->isolate->GetHeapStatistics(&heap);
	if (heap.used_heap_size() + that->extra_allocated_memory > that->memory_limit + that->misc_memory_size) {
		that->RequestMemoryPressureNotification(MemoryPressureLevel::kCritical, true, true);
	} else {
		that->RequestMemoryPressureNotification(MemoryPressureLevel::kModerate, true, true);
	}
	return current_heap_limit + 1024 * 1024 * 1024;
}

void IsolateEnvironment::RequestMemoryPressureNotification(MemoryPressureLevel memory_pressure, bool is_reentrant_gc, bool as_interrupt) {
	// Before commit v8 6.9.406 / 0fb4f6a2a triggering the GC from within a GC callback would output
	// some GC tracing diagnostics. After the commit it is properly gated behind a v8 debug flag.
	if (
#if !V8_AT_LEAST(6, 9, 406)
		is_reentrant_gc ||
#endif
		as_interrupt
	) {
		this->memory_pressure = memory_pressure;
		isolate->RequestInterrupt(MemoryPressureInterrupt, static_cast<void*>(this));
	} else {
		this->memory_pressure = MemoryPressureLevel::kNone;
		isolate->MemoryPressureNotification(memory_pressure);
		if (is_reentrant_gc && memory_pressure == MemoryPressureLevel::kCritical) {
			// Reentrant GC doesn't trigger callbacks
			MarkSweepCompactEpilogue(isolate, GCType::kGCTypeMarkSweepCompact, GCCallbackFlags::kGCCallbackFlagForced, reinterpret_cast<void*>(this));
		}
	}
}

void IsolateEnvironment::MemoryPressureInterrupt(Isolate* /* isolate */, void* data) {
	static_cast<IsolateEnvironment*>(data)->CheckMemoryPressure();
}

void IsolateEnvironment::CheckMemoryPressure() {
	MemoryPressureLevel pressure = memory_pressure;
	if (pressure != MemoryPressureLevel::kNone) {
		memory_pressure = MemoryPressureLevel::kNone;
		isolate->MemoryPressureNotification(pressure);
	}
}

void IsolateEnvironment::AsyncEntry() {
	Executor::Lock lock(*this);
	if (!nodejs_isolate) {
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
			Scheduler::Lock lock{scheduler};
			auto foo = !lock.scheduler.tasks.empty() ? &lock.scheduler.tasks.front() : nullptr;
			tasks = ExchangeDefault(lock.scheduler.tasks);
			assert(tasks.empty() || foo == &tasks.front());
			handle_tasks = ExchangeDefault(lock.scheduler.handle_tasks);
			interrupts = ExchangeDefault(lock.scheduler.interrupts);
			if (tasks.empty() && handle_tasks.empty() && interrupts.empty()) {
				lock.scheduler.DoneRunning();
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
			CheckMemoryPressure();
		}
	}
}

template <std::queue<std::unique_ptr<Runnable>> Scheduler::Implementation::*Tasks>
void IsolateEnvironment::InterruptEntryImplementation() {
	// Executor::Lock is already acquired
	while (true) {
		auto interrupts = [&]() {
			Scheduler::Lock lock{scheduler};
			return ExchangeDefault(lock.scheduler.*Tasks);
		}();
		if (interrupts.empty()) {
			return;
		}
		do {
			interrupts.front()->Run();
			interrupts.pop();
		} while (!interrupts.empty());
	}
}

void IsolateEnvironment::InterruptEntryAsync() {
	return InterruptEntryImplementation<&Scheduler::Implementation::interrupts>();
}

void IsolateEnvironment::InterruptEntrySync() {
	return InterruptEntryImplementation<&Scheduler::Implementation::sync_interrupts>();
}

IsolateEnvironment::IsolateEnvironment() :
	owned_isolates{std::make_unique<OwnedIsolates>()},
	scheduler{*this},
	executor{*this},
	isolate_map{isolate_map_shared} {

}

void IsolateEnvironment::IsolateCtor(Isolate* isolate, Local<Context> context) {
	this->isolate = isolate;
	default_context.Reset(isolate, context);
	nodejs_isolate = true;
}

void IsolateEnvironment::IsolateCtor(size_t memory_limit_in_mb, shared_ptr<void> snapshot_blob, size_t snapshot_length) {
	memory_limit = memory_limit_in_mb * 1024 * 1024;
	allocator_ptr = std::make_unique<LimitedAllocator>(*this, memory_limit);
	snapshot_blob_ptr = std::move(snapshot_blob);
	nodejs_isolate = false;

	// Calculate resource constraints
	ResourceConstraints rc;
	rc.set_max_semi_space_size_in_kb((size_t)std::pow(2, std::min(sizeof(void*) >= 8 ? 4.0 : 3.0, memory_limit_in_mb / 128.0) + 10));
	rc.set_max_old_space_size(
#if V8_AT_LEAST(7, 0, 0)
		memory_limit_in_mb
#else
		// node v10.x seems to not call NearHeapLimit dependably for smaller heap sizes. I'm not sure
		// exactly sure when this was resolved and bisecting on v8 would be frustrating, but since
		// nodejs v11.x it seems ok. For these earlier versions of node the gc epilogue will have to
		// enforce the limit as best it can.
		std::max(size_t{128}, memory_limit_in_mb)
#endif
	);

	// Build isolate from create params
	Isolate::CreateParams create_params;
	create_params.constraints = rc;
	create_params.array_buffer_allocator = allocator_ptr.get();
	if (snapshot_blob_ptr) {
		create_params.snapshot_blob = &startup_data;
		startup_data.data = reinterpret_cast<char*>(snapshot_blob_ptr.get());
		startup_data.raw_size = snapshot_length;
	}
#if V8_AT_LEAST(6, 8, 57)
	isolate = Isolate::Allocate();
	isolate_map->write()->insert(std::make_pair(isolate, this));
	Isolate::Initialize(isolate, create_params);
#else
	isolate = Isolate::New(create_params);
	isolate_map->write()->insert(std::make_pair(isolate, this));
#endif
	// Workaround for bug in snapshot deserializer in v8 in nodejs v10.x
	isolate->SetHostImportModuleDynamicallyCallback(nullptr);
	isolate->SetOOMErrorHandler(OOMErrorCallback);
	isolate->SetPromiseRejectCallback(PromiseRejectCallback);

	// Add GC callbacks
	isolate->AddGCEpilogueCallback(MarkSweepCompactEpilogue, static_cast<void*>(this), GCType::kGCTypeMarkSweepCompact);
	isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, static_cast<void*>(this));

	// Heap statistics crushes down lots of different memory spaces into a single number. We note the
	// difference between the requested old space and v8's calculated heap size.
	HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);
	initial_heap_size_limit = heap.heap_size_limit();
	misc_memory_size = heap.heap_size_limit() - memory_limit_in_mb * 1024 * 1024;

	// Create a default context for the library to use if needed
	{
		Locker locker(isolate);
		HandleScope handle_scope(isolate);
		default_context.Reset(isolate, NewContext());
	}

	// There is no asynchronous Isolate ctor so we should throw away thread specifics in case
	// the client always uses async methods
	isolate->DiscardThreadSpecificMetadata();

	// Save reference to this isolate in the default isolate
	Executor::GetDefaultEnvironment().owned_isolates->write()->insert(holder);
}

IsolateEnvironment::~IsolateEnvironment() {
	if (nodejs_isolate) {
		// Throw away all owned isolates when the root one dies
		auto isolates = *owned_isolates->read(); // copy
		for (auto ii = isolates.begin(); ii != isolates.end(); ) {
			auto ref = ii->lock();
			if (ref) {
				ref->Dispose();
				++ii;
			} else {
				ii = isolates.erase(ii);
			}
		}
		for (auto& holder : isolates) {
			auto ref = holder.lock();
			if (ref) {
				ref->ReleaseAndJoin();
			}
		}
	} else {
		{
			// Grab local pointer to inspector agent with scheduler lock active
			auto agent_ptr = [&]() {
				Scheduler::Lock lock{scheduler};
				return std::move(inspector_agent);
			}();
			// Now activate executor lock and invoke inspector agent's dtor
			Executor::Lock lock{*this};
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
			Scheduler::Lock scheduler_lock{scheduler};
			ExchangeDefault(scheduler_lock.scheduler.interrupts);
			ExchangeDefault(scheduler_lock.scheduler.sync_interrupts);
			ExchangeDefault(scheduler_lock.scheduler.handle_tasks);
			ExchangeDefault(scheduler_lock.scheduler.tasks);
		}
		{
			// Dispose() will call destructors for external strings and array buffers, so this lock sets the
			// "current" isolate for those C++ dtors to function correctly without locking v8
			Executor::Scope lock{*this};
			isolate->Dispose();
		}
		// Unregister from Platform
		isolate_map->write()->erase(isolate);
		// Unreference from default isolate
		executor.default_executor.env.owned_isolates->write()->erase(holder);
	}
	// Send notification that this isolate is totally disposed
	{
		auto ref = holder.lock();
		if (ref) {
			ref->state.write()->is_disposed = true;
			ref->cv.notify_all();
		}
	}
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
	CheckMemoryPressure();
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
	assert(!nodejs_isolate);
	terminated = true;
	{
		Scheduler::Lock lock(scheduler);
		if (inspector_agent) {
			inspector_agent->Terminate();
		}
	}
	isolate->TerminateExecution();
	auto ref = holder.lock();
	if (ref) {
		ref->state.write()->isolate.reset();
	}
}

void IsolateEnvironment::AddWeakCallback(Persistent<Object>* handle, void(*fn)(void*), void* param) {
	if (nodejs_isolate) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it != weak_persistents.end()) {
		throw std::logic_error("Weak callback already added");
	}
	weak_persistents.insert(std::make_pair(handle, std::make_pair(fn, param)));
}

void IsolateEnvironment::RemoveWeakCallback(Persistent<Object>* handle) {
	if (nodejs_isolate) {
		return;
	}
	auto it = weak_persistents.find(handle);
	if (it == weak_persistents.end()) {
		throw std::logic_error("Weak callback doesn't exist");
	}
	weak_persistents.erase(it);
}

shared_ptr<IsolateHolder> IsolateEnvironment::LookupIsolate(Isolate* isolate) {
	auto lock = isolate_map_shared->read();
	auto it = lock->find(isolate);
	if (it == lock->end()) {
		return nullptr;
	} else {
		return it->second->holder.lock();
	}
}

} // namespace ivm
