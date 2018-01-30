#pragma once
#include <node.h>
#include <uv.h>
#include <assert.h>
#include "../thread_pool.h"
#include "../external_copy.h"
#include "../timer.h"
#include "../util.h"
//#include "inspector.h"
#include "runnable.h"
#include "holder.h"

#include "../apply_from_tuple.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>

#include <queue>
#include <vector>
#include <map>

namespace ivm {
using namespace v8;
using std::shared_ptr;
using std::unique_ptr;

/**
 * Wrapper around Isolate with helpers to make working with multiple isolates easier.
 */
class IsolateEnvironment {
	friend class IsolateHolder;
	friend class ThreePhaseTask;

	public:
		/**
		 * ExecutorLock class handles v8 locking while C++ code is running. Thread syncronization is
		 * handled by v8::Locker. This also enters the isolate and sets up a handle scope.
		 */
		class ExecutorLock {
			private:
				static thread_local IsolateEnvironment* current;
				static std::thread::id default_thread;
				IsolateEnvironment* last;
				v8::Locker locker;
				v8::Isolate::Scope isolate_scope;
				v8::HandleScope handle_scope;

			public:
				explicit ExecutorLock(IsolateEnvironment& env);
				ExecutorLock(const ExecutorLock&) = delete;
				ExecutorLock operator= (const ExecutorLock&) = delete;
				~ExecutorLock();
				static IsolateEnvironment* GetCurrent() { return current; }
				static void Init(IsolateEnvironment& default_isolate);
				static bool IsDefaultThread();
		};

		/**
		 * Keeps track of tasks an isolate needs to run and manages its run state (running or waiting).
		 * This does all the interaction with libuv async and the thread pool.
		 */
		class Scheduler {
			public:
				enum class Status { Waiting, Running };

			private:
				static uv_async_t root_async;
				static thread_pool_t thread_pool;
				static std::atomic<int> uv_ref_count;
				Status status = Status::Waiting;
				std::mutex mutex;
				std::queue<unique_ptr<Runnable>> tasks;
				std::queue<unique_ptr<std::function<void()>>> interrupts;
				thread_pool_t::affinity_t thread_affinity;

			public:
				// A Scheduler::Lock is needed to interact with the task queue
				class Lock {
					private:
						Scheduler& scheduler;
						std::unique_lock<std::mutex> lock;
					public:
						explicit Lock(Scheduler& scheduler);
						Lock(const Lock&) = delete;
						Lock operator= (const Lock&) = delete;
						~Lock();
						void DoneRunning();
						// Add work to the task queue
						void PushTask(std::unique_ptr<Runnable> task);
						void PushInterrupt(std::unique_ptr<std::function<void()>> interrupt);
						// Takes control of current tasks. Resets current queue
						std::queue<unique_ptr<Runnable>> TakeTasks();
						std::queue<unique_ptr<std::function<void()>>> TakeInterrupts();
						// Returns true if a wake was scheduled, true if the isolate is already running.
						bool WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate);
				};
				friend class Lock;

				Scheduler();
				Scheduler(const Scheduler&) = delete;
				Scheduler operator= (const Scheduler&) = delete;
				~Scheduler();
				static void Init();

			private:
				static void AsyncCallbackRoot(uv_async_t* async);
				static void AsyncCallbackPool(bool pool_thread, void* param);
		};

	private:
		struct BookkeepingStatics {
			/**
			 * These statics are needed in the destructor to update bookkeeping information. The root
			 * IsolateEnvironment will be be destroyed when the module is being destroyed, and static members
			 * may be destroyed before that happens. So we stash them here and wrap the whole in a
			 * shared_ptr so we can ensure access to them even when the module is being torn down.
			 */
			std::map<Isolate*, IsolateEnvironment*> isolate_map;
			std::mutex lookup_mutex;
		};

		static shared_ptr<BookkeepingStatics> bookkeeping_statics_shared;
		static size_t specifics_count;

		Isolate* isolate;
		Scheduler scheduler;
		std::shared_ptr<IsolateHolder> holder;
//		unique_ptr<InspectorClientImpl> inspector_client;
		Persistent<Context> default_context;
		unique_ptr<ArrayBuffer::Allocator> allocator_ptr;
		shared_ptr<ExternalCopyArrayBuffer> snapshot_blob_ptr;
		StartupData startup_data;
		size_t memory_limit;
		bool hit_memory_limit = false;
		bool root;
		HeapStatistics last_heap;
		shared_ptr<BookkeepingStatics> bookkeeping_statics;
		Persistent<Value> rejected_promise_error;

		std::vector<unique_ptr<Persistent<Value>>> specifics;
		std::vector<unique_ptr<Persistent<FunctionTemplate>>> specifics_ft;
		std::map<Persistent<Object>*, std::pair<void(*)(void*), void*>> weak_persistents;

	public:
		// TODO: These are used in RunWithTimeout
		std::atomic<int> terminate_depth { 0 };
		std::atomic<bool> terminated { false };

	private:

		/**
		 * Catches garbage collections on the isolate and terminates if we use too much.
		 */
		static void GCEpilogueCallback(Isolate* isolate, GCType type, GCCallbackFlags flags) {

			// Get current heap statistics
			auto that = IsolateEnvironment::GetCurrent();
			assert(that->isolate == isolate);
			HeapStatistics heap;
			isolate->GetHeapStatistics(&heap);

			// If we are above the heap limit then kill this isolate
			if (heap.used_heap_size() > that->memory_limit * 1024 * 1024) {
				that->hit_memory_limit = true;
				isolate->TerminateExecution();
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

		/**
		 * Called when an isolate has an uncaught error in a promise. This makes no distinction between
		 * contexts so we have to handle that ourselves.
		 */
		static void PromiseRejectCallback(PromiseRejectMessage rejection) {
			auto that = IsolateEnvironment::GetCurrent();
			assert(that->isolate == Isolate::GetCurrent());
			that->rejected_promise_error.Reset(that->isolate, rejection.GetValue());
		}

	public:
		/**
		 * This is called after user code runs. This throws a fatal error if the memory limit was hit.
		 * If an asyncronous exception (promise) was lost, this will throw it for real.
		 */
		void TaskEpilogue() {
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

		/**
		 * Like thread_local data, but specific to an Isolate instead.
		 */
		template <typename T>
		class IsolateSpecific {
			private:
				template <typename L, typename V, V IsolateEnvironment::*S>
				MaybeLocal<L> Deref() const {
					IsolateEnvironment& isolate = *ExecutorLock::GetCurrent();
					if ((isolate.*S).size() > key) {
						if (!(isolate.*S)[key]->IsEmpty()) {
							return MaybeLocal<L>(Local<L>::New(isolate, *(isolate.*S)[key]));
						}
					}
					return MaybeLocal<L>();
				}

				template <typename L, typename V, V IsolateEnvironment::*S>
				void Reset(Local<L> handle) {
					IsolateEnvironment& isolate = *ExecutorLock::GetCurrent();
					if ((isolate.*S).size() <= key) {
						(isolate.*S).reserve(key + 1);
						while ((isolate.*S).size() <= key) {
							(isolate.*S).emplace_back(std::make_unique<Persistent<L>>());
						}
					}
					(isolate.*S)[key]->Reset(isolate, handle);
				}

			public:
				size_t key;
				IsolateSpecific() : key(++IsolateEnvironment::specifics_count) {}

				MaybeLocal<T> Deref() const {
					Local<Value> local;
					if (Deref<Value, decltype(IsolateEnvironment::specifics), &IsolateEnvironment::specifics>().ToLocal(&local)) {
						return MaybeLocal<T>(Local<Object>::Cast(local));
					} else {
						return MaybeLocal<T>();
					}
				}

				void Reset(Local<T> handle) {
					Reset<Value, decltype(IsolateEnvironment::specifics), &IsolateEnvironment::specifics>(handle);
				}
		};

		/**
		 * Return shared pointer the currently running Isolate's shared pointer
		 */
		static IsolateEnvironment* GetCurrent() {
			return ExecutorLock::GetCurrent();
		}

		static shared_ptr<IsolateHolder> GetCurrentHolder() {
			return ExecutorLock::GetCurrent()->holder;
		}

		Isolate* GetIsolate() {
			return isolate;
		}

		/**
		 * Wrap an existing Isolate. This should only be called for the main node Isolate.
		 */
		IsolateEnvironment(Isolate* isolate, Local<Context> context) :
			isolate(isolate),
			default_context(isolate, context),
			root(true),
			bookkeeping_statics(bookkeeping_statics_shared) {
			ExecutorLock::Init(*this);
			Scheduler::Init();
			std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
			bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
		}

		/**
		 * Create a new wrapped Isolate
		 */
		IsolateEnvironment(
			ResourceConstraints& resource_constraints,
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
			if (snapshot_blob_ptr.get() != nullptr) {
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
				v8::Locker locker(isolate);
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

		/**
		 * Factory method which generates an IsolateHolder.
		 */
		template <typename ...Args>
		static shared_ptr<IsolateHolder> New(Args&&... args) {
			auto isolate = std::make_shared<IsolateEnvironment>(std::forward<Args>(args)...);
			auto holder = std::make_shared<IsolateHolder>(isolate);
			isolate->holder = holder;
			return holder;
		}

		~IsolateEnvironment() {
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

		/**
		 * Convenience operators to work with underlying isolate
		 */
		operator Isolate*() const {
			return isolate;
		}

		Isolate* operator->() const {
			return isolate;
		}

		/**
		 * Fetch heap statistics from v8. This isn't explicitly marked as safe to do without a locker,
		 * but based on the code it'll be fine unless you do something crazy like call Dispose() in the
		 * one nanosecond this is running. And even that should be impossible because of the queue lock
		 * we get.
		 */
		HeapStatistics GetHeapStatistics() {
			HeapStatistics heap;
			isolate->GetHeapStatistics(&heap);
			return heap;
		}

		/**
		 * Get allocator used by this isolate. Will return nullptr for the default isolate.
		 */
		ArrayBuffer::Allocator* GetAllocator() {
			return allocator_ptr.get();
		}

		bool DidHitMemoryLimit() const {
			return hit_memory_limit;
		}

		/**
		 * Ask this isolate to finish everything it's doing.
		 */
		void Terminate() {
			assert(!root);
			terminated = true;
			isolate->TerminateExecution();
		}

		/**
		 * Create a new debug channel
		 */
		/*
		unique_ptr<InspectorSession> CreateInspectorSession(shared_ptr<V8Inspector::Channel> channel) {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
			return inspector_client->createSession(channel);
		}
		*/

		/**
		 * Given a v8 isolate this will find the IsolateEnvironment instance, if any, that belongs to it.
		 */
		static std::shared_ptr<IsolateHolder> LookupIsolate(Isolate* isolate) {
			std::unique_lock<std::mutex> lock(bookkeeping_statics_shared->lookup_mutex);
			auto it = bookkeeping_statics_shared->isolate_map.find(isolate);
			if (it == bookkeeping_statics_shared->isolate_map.end()) {
				return nullptr;
			} else {
				return it->second->holder;
			}
		}

		/**
		 * Schedules a task which will interupt JS execution. This will wake up the isolate if it's not
		 * currently running.
		 */
		/*
		void ScheduleInterrupt(std::function<void()> fn) {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal || hit_memory_limit) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
			interrupt.push(std::make_unique<std::function<void()>>(std::move(fn)));
			if (!WakeIsolate()) {
				isolate->RequestInterrupt(InterruptEntry, this);
			}
		}
		*/

		void AsyncEntry() {
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

		/**
		 * This is called in response to RequestInterrupt. In this case the isolate will already be
		 * locked and enterred.
		 */
		/*
		static void InterruptEntry(Isolate* isolate, void* param) {
			IsolateEnvironment& that = *static_cast<IsolateEnvironment*>(param);
			assert(that.status == Status::Running);
			decltype(interrupt) interrupt_copy;
			{
				std::unique_lock<std::mutex> lock(that.queue_mutex);
				std::swap(that.interrupt, interrupt_copy);
			}
			while (!interrupt_copy.empty()) {
				auto task = std::move(interrupt_copy.front());
				(*task)();
				interrupt_copy.pop();
			}
		}
		*/

		Local<Context> DefaultContext() const {
			return Local<Context>::New(isolate, default_context);
		}

		void ContextCreated(Local<Context> context) {
			std::string name = "<isolated-vm>";
			//inspector_client->inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView((const uint8_t*)name.c_str(), name.length())));
		}

		void ContextDestroyed(Local<Context> context) {
			if (!root) { // TODO: This gets called for the root context because of ShareableContext's dtor :(
				//inspector_client->inspector->contextDestroyed(context);
			}
		}

		/**
		 * Since a created Isolate can be disposed of at any time we need to keep track of weak
		 * persistents to call those destructors on isolate disposal.
		 */
		void AddWeakCallback(Persistent<Object>* handle, void(*fn)(void*), void* param) {
			if (root) return;
			auto it = weak_persistents.find(handle);
			if (it != weak_persistents.end()) {
				throw std::runtime_error("Weak callback already added");
			}
			weak_persistents.insert(std::make_pair(handle, std::make_pair(fn, param)));
		}

		void RemoveWeakCallback(Persistent<Object>* handle) {
			if (root) return;
			auto it = weak_persistents.find(handle);
			if (it == weak_persistents.end()) {
				throw std::runtime_error("Weak callback doesn't exist");
			}
			weak_persistents.erase(it);
		}
};

/**
 * Run some v8 thing with a timeout
 */
template <typename F>
Local<Value> RunWithTimeout(uint32_t timeout_ms, IsolateEnvironment& isolate, F&& fn) {
	bool did_timeout = false, did_finish = false;
	MaybeLocal<Value> result;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, [&did_timeout, &did_finish, &isolate]() {
				did_timeout = true;
				++isolate.terminate_depth;
				isolate->TerminateExecution();
				// FIXME(?): It seems that one call to TerminateExecution() doesn't kill the script if
				// there is a promise handler scheduled. This is unexpected behavior but I can't
				// reproduce it in vanilla v8 so the issue seems more complex. I'm punting on this for
				// now with a hack but will look again when nodejs pulls in a newer version of v8 with
				// more mature microtask support.
				//
				// This loop always terminates for me in 1 iteration but it goes up to 100 because the
				// only other option is terminating the application if an isolate has gone out of
				// control.
				for (int ii = 0; ii < 100; ++ii) {
					std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(2));
					if (did_finish) {
						return;
					}
					isolate->TerminateExecution();
				}
				assert(false);
			});
		}
		result = fn();
		did_finish = true;
	}
	if (isolate.DidHitMemoryLimit()) {
		throw js_fatal_error();
	} else if (did_timeout) {
		if (!isolate.terminated && --isolate.terminate_depth == 0) {
			isolate->CancelTerminateExecution();
		}
		throw js_generic_error("Script execution timed out.");
	}
	return Unmaybe(result);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winstantiation-after-specialization"
// These instantiations make msvc correctly link the template specializations in shareable_isolate.cc, but clang whines about it
template <>
MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const;
template MaybeLocal<FunctionTemplate> IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Deref() const;

template <>
void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);
template void IsolateEnvironment::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);
#pragma clang diagnostic pop

}
