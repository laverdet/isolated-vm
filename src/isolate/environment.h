#pragma once
#include <v8.h>
#include <uv.h>

#include "holder.h"
#include "../thread_pool.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

namespace ivm {

class Runnable;

/**
 * Wrapper around Isolate with helpers to make working with multiple isolates easier.
 */
class IsolateEnvironment {
	// These are here so they can adjust `extra_allocated_memory`. TODO: Make this a method
	friend class ExternalCopyBytes;
	friend class ExternalCopyArrayBuffer;
	friend class ExternalCopySharedArrayBuffer;
	friend class ExternalCopyString;

	friend class InspectorAgent;
	friend class InspectorSession;
	friend class IsolateHolder;
	friend class LimitedAllocator;
	friend class ThreePhaseTask;
	template <typename F>
	friend v8::Local<v8::Value> RunWithTimeout(uint32_t timeout_ms, F&& fn);
	template <typename ...Types>
	friend class RemoteTuple;

	public:
		/**
		 * Executor class handles v8 locking while C++ code is running. Thread syncronization is handled
		 * by v8::Locker. This also enters the isolate and sets up a handle scope.
		 */
		class Executor { // "En taro adun"
			friend class InspectorAgent;
			private:
				struct CpuTimer {
					using TimePoint = std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>;
					struct PauseScope {
						CpuTimer* timer;
						explicit PauseScope(CpuTimer* timer);
						PauseScope(const PauseScope&) = delete;
						PauseScope& operator=(const PauseScope&) = delete;
						~PauseScope();
					};
					struct UnpauseScope {
						CpuTimer* timer;
						explicit UnpauseScope(PauseScope& pause);
						UnpauseScope(const UnpauseScope&) = delete;
						UnpauseScope& operator=(const UnpauseScope&) = delete;
						~UnpauseScope();
					};
					Executor& executor;
					CpuTimer* last;
					TimePoint time;
					explicit CpuTimer(Executor& executor);
					CpuTimer(const CpuTimer&) = delete;
					CpuTimer operator= (const CpuTimer&) = delete;
					~CpuTimer();
					std::chrono::nanoseconds Delta(const std::lock_guard<std::mutex>& /* lock */) const;
					void Pause();
					void Resume();
					static TimePoint Now();
				};

				// WallTimer is also responsible for pausing the current CpuTimer before we attempt to
				// acquire the v8::Locker, because that could block in which case CPU shouldn't be counted.
				struct WallTimer {
					Executor& executor;
					CpuTimer* cpu_timer;
					std::chrono::time_point<std::chrono::steady_clock> time;
					explicit WallTimer(Executor& executor);
					WallTimer(const WallTimer&) = delete;
					WallTimer operator= (const WallTimer&) = delete;
					~WallTimer();
					std::chrono::nanoseconds Delta(const std::lock_guard<std::mutex>& /* lock */) const;
				};

			public:
				class Scope {
					private:
						IsolateEnvironment* last;

					public:
						explicit Scope(IsolateEnvironment& env);
						Scope(const Scope&) = delete;
						Scope operator= (const Scope&) = delete;
						~Scope();
				};

				class Lock {
					private:
						// These need to be separate from `Executor::current` because the default isolate
						// doesn't actually get a lock.
						static thread_local Lock* current;
						Lock* last;
						Scope scope;
						WallTimer wall_timer;
						v8::Locker locker;
						CpuTimer cpu_timer;
						v8::Isolate::Scope isolate_scope;
						v8::HandleScope handle_scope;

					public:
						explicit Lock(IsolateEnvironment& env);
						Lock(const Lock&) = delete;
						Lock operator= (const Lock&) = delete;
						~Lock();
				};

				class Unlock {
					private:
						CpuTimer::PauseScope pause_scope;
						v8::Unlocker unlocker;

					public:
						explicit Unlock(IsolateEnvironment& env);
						Unlock(const Unlock&) = delete;
						Unlock operator= (const Unlock&) = delete;
						~Unlock();
				};

				static thread_local IsolateEnvironment* current_env;
				static std::thread::id default_thread;
				IsolateEnvironment& env;
				Lock* current_lock = nullptr;
				static thread_local CpuTimer* cpu_timer_thread;
				CpuTimer* cpu_timer = nullptr;
				WallTimer* wall_timer = nullptr;
				std::mutex timer_mutex;
				std::chrono::nanoseconds cpu_time{};
				std::chrono::nanoseconds wall_time{};

			public:
				explicit Executor(IsolateEnvironment& env);
				Executor(const Executor&) = delete;
				Executor operator= (const Executor&) = delete;
				~Executor() = default;
				static void Init(IsolateEnvironment& default_isolate);
				static IsolateEnvironment* GetCurrent() { return current_env; }
				static bool IsDefaultThread();
		};

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

		/**
		 * Ensures we don't blow up the v8 heap while transferring arbitrary data
		 */
		class HeapCheck {
			private:
				IsolateEnvironment& env;
				size_t extra_size_before;
			public:
				explicit HeapCheck(IsolateEnvironment& env);
				HeapCheck(const HeapCheck&) = delete;
				HeapCheck& operator= (const HeapCheck&) = delete;
				void Epilogue();
		};

		/**
		 * Like thread_local data, but specific to an Isolate instead.
		 */
		template <typename T>
		class IsolateSpecific {
			private:
				union HandleConvert {
					v8::Local<v8::Data> data;
					v8::Local<T> value;
					explicit HandleConvert(v8::Local<v8::Data> data) : data(data) {}
				};
				size_t key;

			public:
				IsolateSpecific() : key(IsolateEnvironment::specifics_count++) {}

				v8::MaybeLocal<T> Deref() const {
					IsolateEnvironment& env = *Executor::GetCurrent();
					if (env.specifics.size() > key) {
						if (!env.specifics[key]->IsEmpty()) {
							// This is dangerous but `Local` doesn't let you upcast from Data to
							// `FunctionTemplate` or `Private` which is stupid.
							HandleConvert handle(env.specifics[key]->Get(env.isolate));
							return v8::MaybeLocal<T>(handle.value);
						}
					}
					return v8::MaybeLocal<T>();
				}

				void Set(v8::Local<T> handle) {
					IsolateEnvironment& env = *Executor::GetCurrent();
					if (env.specifics.size() <= key) {
						env.specifics.reserve(key + 1);
						while (env.specifics.size() < key) {
							env.specifics.emplace_back(std::make_unique<v8::Eternal<v8::Data>>());
						}
						env.specifics.emplace_back(std::make_unique<v8::Eternal<v8::Data>>(env.isolate, handle));
					} else {
						env.specifics[key]->Set(env.isolate, handle);
					}
				}
		};

	private:
		struct BookkeepingStatics {
			/**
			 * These statics are needed in the destructor to update bookkeeping information. The root
			 * IsolateEnvironment will be be destroyed when the module is being destroyed, and static members
			 * may be destroyed before that happens. So we stash them here and wrap the whole in a
			 * shared_ptr so we can ensure access to them even when the module is being torn down.
			 */
			std::unordered_map<v8::Isolate*, IsolateEnvironment*> isolate_map;
			std::mutex lookup_mutex;
			bool did_shutdown = false;
		};

		static std::shared_ptr<BookkeepingStatics> bookkeeping_statics_shared;
		static size_t specifics_count;

		v8::Isolate* isolate;
		Scheduler scheduler;
		Executor executor;
		std::shared_ptr<IsolateHolder> holder;
		std::unique_ptr<class InspectorAgent> inspector_agent;
		v8::Persistent<v8::Context> default_context;
		std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_ptr;
		std::shared_ptr<void> snapshot_blob_ptr;
		v8::StartupData startup_data {};
		void* timer_holder = nullptr;
		size_t memory_limit = 0;
		size_t extra_allocated_memory = 0;
		bool hit_memory_limit = false;
		bool root;
		std::atomic<unsigned int> remotes_count{0};
		v8::HeapStatistics last_heap {};
		std::shared_ptr<BookkeepingStatics> bookkeeping_statics;
		v8::Persistent<v8::Value> rejected_promise_error;

		std::vector<std::unique_ptr<v8::Eternal<v8::Data>>> specifics;
		std::unordered_map<v8::Persistent<v8::Object>*, std::pair<void(*)(void*), void*>> weak_persistents;

	public:
		std::unordered_multimap<int, struct ModuleInfo*> module_handles;
		std::unordered_map<class NativeModule*, std::shared_ptr<NativeModule>> native_modules;
		std::atomic<int> terminate_depth { 0 };
		std::atomic<bool> terminated { false };

	private:

		/**
		 * If this function is called then I have failed you.
		 */
		static void OOMErrorCallback(const char* location, bool is_heap_oom);

		/**
		 * Called when an isolate has an uncaught error in a promise. This makes no distinction between
		 * contexts so we have to handle that ourselves.
		 */
		static void PromiseRejectCallback(v8::PromiseRejectMessage rejection);

		/**
		 * Called by v8 when this isolate is about to hit the heap limit (node v10.4.0 and above)
		 */
		static size_t NearHeapLimitCallback(void* data, size_t current_heap_limit, size_t initial_heap_limit);

		/**
		 * Called by Scheduler when there is work to be done in this isolate.
		 */
		void AsyncEntry();
		template <std::queue<std::unique_ptr<Runnable>> (Scheduler::Lock::*Take)()>
		void InterruptEntry();

		/**
		 * Wrap an existing Isolate. This should only be called for the main node Isolate.
		 */
		void IsolateCtor(v8::Isolate* isolate, v8::Local<v8::Context> context);

		/**
		 * Create a new wrapped Isolate.
		 */
		void IsolateCtor(size_t memory_limit, std::shared_ptr<void> snapshot_blob, size_t snapshot_length);

	public:
		/**
		 * The constructor should be called through the factory.
		 */
		IsolateEnvironment();
		IsolateEnvironment(const IsolateEnvironment&) = delete;
		IsolateEnvironment operator= (const IsolateEnvironment&) = delete;
		~IsolateEnvironment();

		/**
		 * Factory method which generates an IsolateHolder.
		 */
		template <typename ...Args>
		static std::shared_ptr<IsolateHolder> New(Args&&... args) {
			auto isolate = std::make_shared<IsolateEnvironment>();
			auto holder = std::make_shared<IsolateHolder>(isolate);
			isolate->holder = holder;
			isolate->IsolateCtor(std::forward<Args>(args)...);
			return holder;
		}

		/**
		 * Return pointer the currently running IsolateEnvironment
		 */
		static IsolateEnvironment* GetCurrent() {
			return Executor::GetCurrent();
		}

		/**
		 * Return shared_ptr to current IsolateHolder
		 */
		static std::shared_ptr<IsolateHolder> GetCurrentHolder() {
			return Executor::GetCurrent()->holder;
		}

		/**
		 * Convenience operators to work with underlying isolate
		 */
		operator v8::Isolate*() const { // NOLINT
			return isolate;
		}

		v8::Isolate* operator->() const { // Should probably remove this one..
			return isolate;
		}

		v8::Isolate* GetIsolate() const {
			return isolate;
		}

		/**
		 * Default context, useful for generating certain objects when we aren't in a context.
		 */
		v8::Local<v8::Context> DefaultContext() const {
			return v8::Local<v8::Context>::New(isolate, default_context);
		}

		/**
		 * Creates a new context. Must be used instead of Context::New() because of snapshot deserialization
		 */
		v8::Local<v8::Context> NewContext();

		/**
		 * This is called after user code runs. This throws a fatal error if the memory limit was hit.
		 * If an asyncronous exception (promise) was lost, this will throw it for real.
		 */
		void TaskEpilogue();

		/**
		 * Get allocator used by this isolate. Will return nullptr for the default isolate.
		 */
		v8::ArrayBuffer::Allocator* GetAllocator() const {
			return allocator_ptr.get();
		}

		/**
		 * Get the set memory limit for this environment
		 */
		size_t GetMemoryLimit() const {
			return memory_limit;
		}

		/**
		 * Enables the inspector for this isolate.
		 */
		void EnableInspectorAgent();

		/**
		 * Returns the InspectorAgent for this Isolate.
		 */
		InspectorAgent* GetInspectorAgent() const;

		/**
		 * Check memory limit flag
		 */
		bool DidHitMemoryLimit() const {
			return hit_memory_limit;
		}

		/**
		 * Not to be confused with v8's `ExternalAllocatedMemory`. This counts up how much memory this
		 * isolate is holding onto outside of v8's heap, even if that memory is shared amongst other
		 * isolates.
		 */
		size_t GetExtraAllocatedMemory() const {
			return extra_allocated_memory;
		}

		/**
		 * Returns the current number of outstanding RemoteHandles<> to this isolate.
		 */
		unsigned int GetRemotesCount() const {
			return remotes_count.load();
		}

		/**
		 * Is this the default nodejs isolate?
		 */
		bool IsDefault() const {
			return root;
		}

		/**
		 * Timer getters
		 */
		std::chrono::nanoseconds GetCpuTime();
		std::chrono::nanoseconds GetWallTime();

		/**
		 * Ask this isolate to finish everything it's doing.
		 */
		void Terminate();

		/**
		 * Cancels an async three_phase_runner if one exists, i.e. applySyncPromise
		 */
		void CancelAsync() {
			Scheduler::Lock lock(scheduler);
			if (scheduler.async_wait != nullptr) {
				scheduler.async_wait->Wake();
			}
		}

		/**
		 * Since a created Isolate can be disposed of at any time we need to keep track of weak
		 * persistents to call those destructors on isolate disposal.
		 */
		void AddWeakCallback(v8::Persistent<v8::Object>* handle, void(*fn)(void*), void* param);
		void RemoveWeakCallback(v8::Persistent<v8::Object>* handle);

		/**
		 * Given a v8 isolate this will find the IsolateEnvironment instance, if any, that belongs to it.
		 */
		static std::shared_ptr<IsolateHolder> LookupIsolate(v8::Isolate* isolate);
};

} // namespace ivm
