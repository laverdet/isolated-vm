#pragma once
#include "runnable.h"
#include "../lib/thread_pool.h"
#include <uv.h>
#include <condition_variable>
#include <mutex>
#include <queue>

namespace ivm {
class IsolateEnvironment;

/**
 * Keeps track of tasks an isolate needs to run and manages its run state (running or waiting).
 * This does all the interaction with libuv async and the thread pool.
 */
class Scheduler {
	friend IsolateEnvironment;
	public:
		enum class Status { Waiting, Running };

		// A Scheduler::Lock is needed to interact with the task queue
		class Lock {
			friend class AsyncWait;
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
				void PushHandleTask(std::unique_ptr<Runnable> handle_task);
				void PushInterrupt(std::unique_ptr<Runnable> interrupt);
				void PushSyncInterrupt(std::unique_ptr<Runnable> interrupt);
				// Takes control of current tasks. Resets current queue
				std::queue<std::unique_ptr<Runnable>> TakeTasks();
				std::queue<std::unique_ptr<Runnable>> TakeHandleTasks();
				std::queue<std::unique_ptr<Runnable>> TakeInterrupts();
				std::queue<std::unique_ptr<Runnable>> TakeSyncInterrupts();
				// Returns true if a wake was scheduled, true if the isolate is already running.
				bool WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate_ptr);
				// Request an interrupt in this isolate. `status` must == Running to invoke this.
				void InterruptIsolate(IsolateEnvironment& isolate);
				// Interrupts an isolate running in the default thread
				void InterruptSyncIsolate(IsolateEnvironment& isolate);
		};

		// Scheduler::AsyncWait will pause the current thread until woken up by another thread
		class AsyncWait {
			private:
				Scheduler& scheduler;
				bool done = false;
				bool ready = false;
			public:
				explicit AsyncWait(Scheduler& scheduler);
				AsyncWait(const AsyncWait&) = delete;
				AsyncWait& operator= (const AsyncWait&) = delete;
				~AsyncWait();
				void Ready();
				void Wait();
				void Wake();
		};

	private:
		static uv_async_t root_async;
		static thread_pool_t thread_pool;
		static std::atomic<unsigned int> uv_ref_count;
		static Scheduler* default_scheduler;
		Status status = Status::Waiting;
		std::mutex mutex;
		std::mutex wait_mutex;
		std::condition_variable_any wait_cv;
		std::queue<std::unique_ptr<Runnable>> tasks;
		std::queue<std::unique_ptr<Runnable>> handle_tasks;
		std::queue<std::unique_ptr<Runnable>> interrupts;
		std::queue<std::unique_ptr<Runnable>> sync_interrupts;
		thread_pool_t::affinity_t thread_affinity;
		AsyncWait* async_wait = nullptr;

	public:
		Scheduler();
		Scheduler(const Scheduler&) = delete;
		Scheduler operator= (const Scheduler&) = delete;
		~Scheduler();
		static void Init(IsolateEnvironment& default_isolate);
		/**
		 * Use to ref/unref the uv handle from C++ API
		 */
		static void IncrementUvRef();
		static void DecrementUvRef();

	private:
		static void AsyncCallbackCommon(bool pool_thread, void* param);
		static void AsyncCallbackDefaultIsolate(uv_async_t* async);
		static void AsyncCallbackNonDefaultIsolate(bool pool_thread, void* param);
		static void AsyncCallbackInterrupt(v8::Isolate* isolate_ptr, void* env_ptr);
		static void SyncCallbackInterrupt(v8::Isolate* isolate_ptr, void* env_ptr);
};

} // namespace ivm
