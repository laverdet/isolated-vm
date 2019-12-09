#pragma once
#include <v8.h>
#include <uv.h>

#include "executor.h"
#include "holder.h"
#include "runnable.h"
#include "scheduler.h"
#include "../lib/lockable.h"
#include "../lib/thread_pool.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <set>
#include <vector>

namespace ivm {

/**
 * Wrapper around Isolate with helpers to make working with multiple isolates easier.
 */
class IsolateEnvironment {
	// These are here so they can adjust `extra_allocated_memory`. TODO: Make this a method
	friend class ExternalCopyBytes;
	friend class ExternalCopyArrayBuffer;
	friend class ExternalCopySharedArrayBuffer;
	friend class ExternalCopyString;

	friend class Executor;
	friend class InspectorAgent;
	friend class InspectorSession;
	friend class IsolateHolder;
	friend class LimitedAllocator;
	friend class Scheduler;
	friend class ThreePhaseTask;
	template <typename F>
	friend v8::Local<v8::Value> RunWithTimeout(uint32_t timeout_ms, F&& fn);

	public:
		/**
		 * Ensures we don't blow up the v8 heap while transferring arbitrary data
		 */
		class HeapCheck {
			private:
				IsolateEnvironment& env;
				size_t extra_size_before;
				bool force;
			public:
				explicit HeapCheck(IsolateEnvironment& env, bool force = false);
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
					IsolateEnvironment& env = *Executor::GetCurrentEnvironment();
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
					IsolateEnvironment& env = *Executor::GetCurrentEnvironment();
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
		template <class Type>
		struct WeakPtrCompare {
			bool operator()(const std::weak_ptr<Type>& left, const std::weak_ptr<Type>& right) const {
				return left.owner_before(right);
			}
		};
		// Another good candidate for std::optional<> (because this is only used by the root isolate)
		using OwnedIsolates = lockable_t<std::set<std::weak_ptr<IsolateHolder>, WeakPtrCompare<IsolateHolder>>, true>;
		std::unique_ptr<OwnedIsolates> owned_isolates;
		static size_t specifics_count;

		v8::Isolate* isolate;
		Scheduler scheduler;
		Executor executor;
		std::weak_ptr<IsolateHolder> holder;
		std::shared_ptr<IsolateTaskRunner> task_runner;
		std::unique_ptr<class InspectorAgent> inspector_agent;
		v8::Persistent<v8::Context> default_context;
		std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_ptr;
		std::shared_ptr<void> snapshot_blob_ptr;
		v8::StartupData startup_data;
		void* timer_holder = nullptr;
		size_t memory_limit = 0;
		size_t initial_heap_size_limit = 0;
		size_t misc_memory_size = 0;
		size_t extra_allocated_memory = 0;
		v8::MemoryPressureLevel memory_pressure = v8::MemoryPressureLevel::kNone;
		bool hit_memory_limit = false;
		bool did_adjust_heap_limit = false;
		bool nodejs_isolate;
		std::atomic<unsigned int> remotes_count{0};
		v8::HeapStatistics last_heap {};
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
		 * GC hooks to kill this isolate before it runs out of memory
		 */
		static void MarkSweepCompactEpilogue(v8::Isolate* isolate, v8::GCType gc_type, v8::GCCallbackFlags gc_flags, void* data);
		static size_t NearHeapLimitCallback(void* data, size_t current_heap_limit, size_t initial_heap_limit);
		void RequestMemoryPressureNotification(v8::MemoryPressureLevel memory_pressure, bool is_reentrant_gc = false, bool as_interrupt = false);
		static void MemoryPressureInterrupt(v8::Isolate* isolate, void* data);
		void CheckMemoryPressure();

		/**
		 * Wrap an existing Isolate. This should only be called for the main node Isolate.
		 */
		void IsolateCtor(v8::Isolate* isolate, v8::Local<v8::Context> context);

		/**
		 * Create a new wrapped Isolate.
		 */
		void IsolateCtor(size_t memory_limit_in_mb, std::shared_ptr<void> snapshot_blob, size_t snapshot_length);

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
			return Executor::GetCurrentEnvironment();
		}

		/**
		 * Return shared_ptr to current IsolateHolder
		 */
		static std::shared_ptr<IsolateHolder> GetCurrentHolder() {
			return Executor::GetCurrentEnvironment()->holder.lock();
		}

		auto GetScheduler() -> Scheduler& {
			return scheduler;
		}

		auto GetTaskRunner() -> const std::shared_ptr<IsolateTaskRunner>& {
			return task_runner;
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
		 * Called by Scheduler when there is work to be done in this isolate.
		 */
		void AsyncEntry();
	private:
		template <std::queue<std::unique_ptr<Runnable>> Scheduler::Implementation::*Tasks>
		void InterruptEntryImplementation();
	public:
		void InterruptEntryAsync();
		void InterruptEntrySync();

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
		 * Get the initial v8 heap_size_limit when the isolate was created.
		 */
		size_t GetInitialHeapSizeLimit() const {
			return initial_heap_size_limit;
		}

		/**
		 * Enables the inspector for this isolate.
		 */
		void EnableInspectorAgent();

		/**
		 * Returns the InspectorAgent for this Isolate.
		 */
		InspectorAgent* GetInspectorAgent() const;

		// Return IsolateHolder
		auto GetHolder() { return holder; }

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
		void AdjustRemotes(int delta) {
			remotes_count.fetch_add(delta);
		}

		/**
		 * Is this the default nodejs isolate?
		 */
		bool IsDefault() const {
			return nodejs_isolate;
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
			Scheduler::Lock lock{scheduler};
			lock.scheduler.CancelAsync();
		}

		/**
		 * Since a created Isolate can be disposed of at any time we need to keep track of weak
		 * persistents to call those destructors on isolate disposal.
		 */
		void AddWeakCallback(v8::Persistent<v8::Object>* handle, void(*fn)(void*), void* param);
		void RemoveWeakCallback(v8::Persistent<v8::Object>* handle);
};

} // namespace ivm
