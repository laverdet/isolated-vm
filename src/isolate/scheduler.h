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
		class AsyncWait;
		class Lock;

	private:
		// This is exposed after instantiating a Scheduler::Lock
		class Implementation {
			friend AsyncWait;
			friend Lock;
			public:
				explicit Implementation(IsolateEnvironment& env) : env{env} {}
				Implementation(const Implementation&) = delete;
				~Implementation() = default;
				auto operator= (const Implementation&) = delete;

				void CancelAsync();
				// Called after AsyncEntry finishes
				void DoneRunning();
				// Request an interrupt in this isolate. `status` must == Running to invoke this.
				void InterruptIsolate();
				// Interrupts an isolate running in the default thread
				void InterruptSyncIsolate();
				// Returns true if a wake was scheduled, false if the isolate is already running.
				auto WakeIsolate(std::shared_ptr<IsolateEnvironment> isolate_ptr) -> bool;

				// Task queues
				std::queue<std::unique_ptr<Runnable>> tasks;
				std::queue<std::unique_ptr<Runnable>> handle_tasks;
				std::queue<std::unique_ptr<Runnable>> interrupts;
				std::queue<std::unique_ptr<Runnable>> sync_interrupts;

			private:
				enum class Status { Waiting, Running };
				IsolateEnvironment& env;
				std::mutex mutex;
				std::mutex wait_mutex;
				std::condition_variable wait_cv;
				thread_pool_t::affinity_t thread_affinity;
				AsyncWait* async_wait = nullptr;
				Status status = Status::Waiting;
		};

	public:
		explicit Scheduler(IsolateEnvironment& isolate) : impl{isolate} {}
		Scheduler(const Scheduler&) = delete;
		~Scheduler() = default;
		auto operator= (const Scheduler&) = delete;

		static void Init(IsolateEnvironment& default_isolate);

		// Used to ref/unref the uv handle from C++ API
		static void IncrementUvRef();
		static void DecrementUvRef();

		// Scheduler::AsyncWait will pause the current thread until woken up by another thread
		class AsyncWait {
			public:
				explicit AsyncWait(Scheduler& scheduler);
				AsyncWait(const AsyncWait&);
				~AsyncWait();
				auto operator= (const AsyncWait&) = delete;

				void Ready();
				void Wait();
				void Wake();

			private:
				Scheduler& scheduler;
				bool done = false;
				bool ready = false;
		};

		// A Scheduler::Lock is needed to interact with the task queue
		class Lock {
			public:
				explicit Lock(Scheduler& scheduler) : scheduler{scheduler.impl}, lock{scheduler.impl.mutex} {}
				Lock(const Lock&) = delete;
				~Lock() = default;
				auto operator= (const Lock&) = delete;
				Implementation& scheduler;

			private:
				std::lock_guard<std::mutex> lock;
		};

	private:
		Implementation impl;
};

} // namespace ivm
