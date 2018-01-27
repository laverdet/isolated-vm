#pragma once
#include <node.h>
#include <uv.h>
#include <assert.h>
#include "thread_pool.h"
#include "external_copy.h"
#include "timer.h"
#include "util.h"
#include "inspector.h"

#include "apply_from_tuple.h"
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
using std::unique_ptr;

/**
 * Wrapper around Isolate with helpers to make working with multiple isolates easier.
 */
class ShareableIsolate : public std::enable_shared_from_this<ShareableIsolate> {
	private:
		struct BookkeepingStatics {
			/**
			 * These statics are needed in the destructor to update bookkeeping information. The root
			 * ShareableIsolate will be be destroyed when the module is being destroyed, and static members
			 * may be destroyed before that happens. So we stash them here and wrap the whole in a
			 * shared_ptr so we can ensure access to them even when the module is being torn down.
			 */
			std::map<Isolate*, ShareableIsolate*> isolate_map;
			std::mutex lookup_mutex;
		};
		static shared_ptr<BookkeepingStatics> bookkeeping_statics_shared;
		static thread_local ShareableIsolate* current;
		static size_t specifics_count;
		static thread_pool_t thread_pool;
		static uv_async_t root_async;
		static std::thread::id default_thread;
		static std::atomic<int> uv_refs;

		Isolate* isolate;
		unique_ptr<InspectorClientImpl> inspector_client;
		Persistent<Context> default_context;
		unique_ptr<ArrayBuffer::Allocator> allocator_ptr;
		shared_ptr<ExternalCopyArrayBuffer> snapshot_blob_ptr;
		StartupData startup_data;
		enum class LifeCycle { Normal, Disposing, Disposed };
		LifeCycle life_cycle;
		enum class Status { Waiting, Running };
		Status status;
		size_t memory_limit;
		bool hit_memory_limit;
		bool root;
		HeapStatistics last_heap;
		shared_ptr<BookkeepingStatics> bookkeeping_statics;
		Persistent<Value> rejected_promise_error;

		std::mutex exec_mutex; // mutex for execution
		std::mutex queue_mutex; // mutex for queueing work
		std::queue<unique_ptr<std::function<void()>>> tasks;
		std::queue<unique_ptr<std::function<void()>>> handle_tasks;
		std::queue<unique_ptr<std::function<void()>>> interrupt_tasks;
		std::vector<unique_ptr<Persistent<Value>>> specifics;
		std::vector<unique_ptr<Persistent<FunctionTemplate>>> specifics_ft;
		std::map<Persistent<Object>*, std::pair<void(*)(void*), void*>> weak_persistents;
		thread_pool_t::affinity_t thread_affinity;

		/**
		 * Wrapper around Locker that sets our own version of `current`. Also makes a HandleScope and
		 * Isolate::Scope.
		 */
		class LockerHelper {
			private:
				ShareableIsolate* last;
				Locker locker;
				Isolate::Scope isolate_scope;
				HandleScope handle_scope;

			public:
				LockerHelper(ShareableIsolate& isolate) :
					last(current), locker(isolate),
					isolate_scope(isolate), handle_scope(isolate) {
					current = &isolate;
				}

				~LockerHelper() {
					current = last;
				}
		};

		/**
		 * Catches garbage collections on the isolate and terminates if we use too much.
		 */
		static void GCEpilogueCallback(Isolate* isolate, GCType type, GCCallbackFlags flags) {

			// Get current heap statistics
			ShareableIsolate& that = ShareableIsolate::GetCurrent();
			assert(that.isolate == isolate);
			HeapStatistics heap;
			isolate->GetHeapStatistics(&heap);

			// If we are above the heap limit then kill this isolate
			if (heap.used_heap_size() > that.memory_limit * 1024 * 1024) {
				that.hit_memory_limit = true;
				isolate->TerminateExecution();
				return;
			}

			// Ask for a deep clean at 80% heap
			if (
				heap.used_heap_size() * 1.25 > that.memory_limit * 1024 * 1024 &&
				that.last_heap.used_heap_size() * 1.25 < that.memory_limit * 1024 * 1024
			) {
				that.ScheduleHandleTask(false, [isolate]() {
					isolate->LowMemoryNotification();
				});
			}

			that.last_heap = heap;
		}

		/**
		 * Called when an isolate has an uncaught error in a promise. This makes no distinction between
		 * contexts so we have to handle that ourselves.
		 */
		static void PromiseRejectCallback(PromiseRejectMessage rejection) {
			ShareableIsolate& that = ShareableIsolate::GetCurrent();
			assert(that.isolate == Isolate::GetCurrent());
			that.rejected_promise_error.Reset(that.isolate, rejection.GetValue());
		}

		/**
		 * Schedules this isolate to wake up and run tasks. If this returns false then the isolate is
		 * already awake. queue_mutex must be held when calling this function.
		 */
		bool WakeIsolate() {
			if (status == Status::Waiting) {
				status = Status::Running;
				if (++uv_refs == 1) {
					uv_ref((uv_handle_t*)&root_async);
				}
				if (root) {
					root_async.data = this;
					uv_async_send(&root_async);
				} else {
					thread_pool.exec(thread_affinity, WorkerEntry, this);
				}
				return true;
			}
			return false;
		}

	public:
		/**
		 * Helper used in Locker to throw if memory_limit was hit, but also return value of function if
		 * not (even if T == void)
		 */
		template <typename T>
		T TaskWrapper(T value) {
			ExecuteHandleTasks();
			isolate->RunMicrotasks();
			if (hit_memory_limit) {
				throw js_error_base();
			}
			if (!rejected_promise_error.IsEmpty()) {
				Context::Scope context_scope(DefaultContext());
				isolate->ThrowException(Local<Value>::New(isolate, rejected_promise_error));
				rejected_promise_error.Reset();
				throw js_error_base();
			}
			return std::move(value);
		}

		/**
		 * Like thread_local data, but specific to an Isolate instead.
		 */
		template <typename T>
		class IsolateSpecific {
			private:
				template <typename L, typename V, V ShareableIsolate::*S>
				MaybeLocal<L> Deref() const {
					ShareableIsolate& isolate = *current;
					if ((isolate.*S).size() > key) {
						if (!(isolate.*S)[key]->IsEmpty()) {
							return MaybeLocal<L>(Local<L>::New(isolate, *(isolate.*S)[key]));
						}
					}
					return MaybeLocal<L>();
				}

				template <typename L, typename V, V ShareableIsolate::*S>
				void Reset(Local<L> handle) {
					ShareableIsolate& isolate = *current;
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
				IsolateSpecific() : key(++ShareableIsolate::specifics_count) {}

				MaybeLocal<T> Deref() const {
					Local<Value> local;
					if (Deref<Value, decltype(ShareableIsolate::specifics), &ShareableIsolate::specifics>().ToLocal(&local)) {
						return MaybeLocal<T>(Local<Object>::Cast(local));
					} else {
						return MaybeLocal<T>();
					}
				}

				void Reset(Local<T> handle) {
					Reset<Value, decltype(ShareableIsolate::specifics), &ShareableIsolate::specifics>(handle);
				}
		};

		/**
		 * Return shared pointer the currently running Isolate's shared pointer
		 */
		static ShareableIsolate& GetCurrent() {
			return *current;
		}

		/**
		 * Wrap an existing Isolate. This should only be called for the main node Isolate.
		 */
		ShareableIsolate(Isolate* isolate, Local<Context> context) :
			isolate(isolate),
			default_context(isolate, context),
			life_cycle(LifeCycle::Normal),
			status(Status::Waiting),
			hit_memory_limit(false),
			root(true),
			bookkeeping_statics(bookkeeping_statics_shared) {
			assert(current == nullptr);
			current = this;
			uv_async_init(uv_default_loop(), &root_async, WorkerEntryRoot);
			uv_unref((uv_handle_t*)&root_async);
			default_thread = std::this_thread::get_id();
			{
				std::unique_lock<std::mutex> lock(bookkeeping_statics->lookup_mutex);
				bookkeeping_statics->isolate_map.insert(std::make_pair(isolate, this));
			}
		}

		/**
		 * Create a new wrapped Isolate
		 */
		ShareableIsolate(
			ResourceConstraints& resource_constraints,
			unique_ptr<ArrayBuffer::Allocator> allocator,
			shared_ptr<ExternalCopyArrayBuffer> snapshot_blob,
			size_t memory_limit
		) :
			allocator_ptr(std::move(allocator)),
			snapshot_blob_ptr(std::move(snapshot_blob)),
			life_cycle(LifeCycle::Normal),
			status(Status::Waiting),
			memory_limit(memory_limit),
			hit_memory_limit(false),
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
			inspector_client = std::make_unique<InspectorClientImpl>(isolate, this);

			// Create a default context for the library to use if needed
			{
				v8::Locker locker(isolate);
				HandleScope handle_scope(isolate);
				Local<Context> context = Context::New(isolate);
				default_context.Reset(isolate, context);
				std::string name = "default";
				inspector_client->inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView((const uint8_t*)name.c_str(), name.length())));
			}

			// There is no asynchronous Isolate ctor so we should throw away thread specifics in case
			// the client always uses async methods
			isolate->DiscardThreadSpecificMetadata();
		}

		~ShareableIsolate() {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (!root && life_cycle == LifeCycle::Normal) {
				lock.unlock();
				Dispose();
			}
			std::unique_lock<std::mutex> other_lock(bookkeeping_statics->lookup_mutex);
			bookkeeping_statics->isolate_map.erase(bookkeeping_statics->isolate_map.find(isolate));
		}

		/**
		 * Convenience operators to work with underlying isolate
		 */
		operator Isolate*() const {
			// TODO: This function is used to check if two isolates are the same, which is valid to run
			// after the isolate is disposed, but not good to have sitting around.
			// assert(life_cycle != LifeCycle::Disposed);
			return isolate;
		}

		Isolate* operator->() const {
			return isolate;
		}

		/**
		 * Get a copy of our shared_ptr<> to this isolate
		 */
		std::shared_ptr<ShareableIsolate> GetShared() {
			return shared_from_this();
		}

		/**
		 * Fetch heap statistics from v8. This isn't explicitly marked as safe to do without a locker,
		 * but based on the code it'll be fine unless you do something crazy like call Dispose() in the
		 * one nanosecond this is running. And even that should be impossible because of the queue lock
		 * we get.
		 */
		HeapStatistics GetHeapStatistics() {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
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

		bool IsNormalLifeCycle() const {
			return life_cycle == LifeCycle::Normal && !hit_memory_limit;
		}

		/**
		 * Dispose of an isolate and clean up dangling handles
		 */
		void Dispose() {
			if (root) {
				throw js_generic_error("Cannot dispose root isolate");
			}
			if (v8::Locker::IsLocked(isolate)) {
				throw js_generic_error("Cannot dispose entered isolate");
			}
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				if (life_cycle != LifeCycle::Normal) {
					lock.unlock();
					throw js_generic_error("Isolate is already disposed or disposing");
				}
				if (!tasks.empty()) {
					lock.unlock();
					throw js_generic_error("Isolate is still busy, let all promises resolve before calling dispose()");
				}
				life_cycle = LifeCycle::Disposing;
				isolate->TerminateExecution();
			}
			std::unique_lock<std::mutex> lock(exec_mutex);
			{
				assert(tasks.empty());
				LockerHelper locker(*this);
				// Dispose of inspector first
				inspector_client.reset();
				// Flush tasks
				ExecuteHandleTasks();
				assert(handle_tasks.empty());
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
			}
			isolate->Dispose();
			life_cycle = LifeCycle::Disposed;
			assert(handle_tasks.empty());
			assert(tasks.empty());
		}

		/**
		 * Create a new debug channel
		 */
		unique_ptr<InspectorSession> CreateInspectorSession(shared_ptr<V8Inspector::Channel> channel) {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
			return inspector_client->createSession(channel);
		}

		/**
		 * Given a v8 isolate this will find the ShareableIsolate instance, if any, that belongs to it.
		 */
		static ShareableIsolate* LookupIsolate(Isolate* isolate) {
			{
				std::unique_lock<std::mutex> lock(bookkeeping_statics_shared->lookup_mutex);
				auto it = bookkeeping_statics_shared->isolate_map.find(isolate);
				if (it == bookkeeping_statics_shared->isolate_map.end()) {
					return nullptr;
				} else {
					return it->second;
				}
			}
		}

		/**
		 * Schedule complex work to run when this isolate is locked. If the isolate is already locked
		 * the task will be run before unlocking, but after the current tasks have finished.
		 */
		template <typename F, typename ...Args>
		void ScheduleTask(F&& fn, Args&&... args) {
			auto fn_ptr = std::make_unique<std::function<void()>>(std::bind(std::forward<F>(fn), std::forward<Args>(args)...));
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal || hit_memory_limit) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
			this->tasks.push(std::move(fn_ptr));
			WakeIsolate();
		}

		/**
		 * Schedules a simple task which is guaranteed to run before this isolate is disposed. Returns
		 * false if isolate is already disposed.
		 */
		bool ScheduleHandleTask(bool run_inline, std::function<void()> fn) {
			if (run_inline && &GetCurrent() == this) {
				fn();
				return true;
			}
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal || hit_memory_limit) {
				return false;
			}
			this->handle_tasks.push(std::make_unique<std::function<void()>>(std::move(fn)));
			return true;
		}

		/**
		 * Schedules a task which will interupt JS execution. This will wake up the isolate if it's not
		 * currently running.
		 */
		void ScheduleInterrupt(std::function<void()> fn) {
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal || hit_memory_limit) {
				lock.unlock();
				throw js_generic_error("Isolate is disposed or disposing");
			}
			interrupt_tasks.push(std::make_unique<std::function<void()>>(std::move(fn)));
			if (!WakeIsolate()) {
				isolate->RequestInterrupt(InterruptEntry, this);
			}
		}

		void ExecuteHandleTasks() {
			decltype(this->handle_tasks) handle_tasks;
			while (true) {
				{
					std::unique_lock<std::mutex> lock(queue_mutex);
					std::swap(handle_tasks, this->handle_tasks);
				}
				if (handle_tasks.empty()) {
					return;
				}
				do {
					auto task = std::move(handle_tasks.front());
					(*task)();
					handle_tasks.pop();
				} while (!handle_tasks.empty());
			}
		}

		static void UnrefUv() {
			if (--uv_refs == 0) {
				uv_unref((uv_handle_t*)&root_async);
				// If we are the last ones to unref then node doesn't exit unless someone else
				// unrefs?? This seems like an obvious libuv bug but I don't want to investigate.
				root_async.data = nullptr;
				uv_async_send(&root_async);
			}
		}

		static void WorkerEntryRoot(uv_async_t* async) {
			if (async->data != nullptr) { // see WorkerEntry nullptr async message
				WorkerEntry(true, async->data);
			}
		}

		static void WorkerEntry(bool pool_thread, void* param) {
			ShareableIsolate& that = *static_cast<ShareableIsolate*>(param);
			auto that_ref = that.GetShared(); // Make sure we don't get deleted by another thread while running
			assert(that.status == Status::Running);
			std::unique_lock<std::mutex> exec_lock(that.exec_mutex);
			{
				LockerHelper locker(that);
				while (true) {
					decltype(that.tasks) tasks;
					decltype(that.handle_tasks) handle_tasks;
					decltype(that.interrupt_tasks) interrupt_tasks;
					{
						std::unique_lock<std::mutex> lock(that.queue_mutex);
						std::swap(tasks, that.tasks);
						std::swap(handle_tasks, that.handle_tasks);
						std::swap(interrupt_tasks, that.interrupt_tasks);
						if (handle_tasks.empty() && interrupt_tasks.empty() && tasks.empty()) {
							that.status = Status::Waiting;
							if (!pool_thread) {
								// In this case the thread pool was full so this loop was run in a temporary thread
								// that will be thrown away after this function finishes. Throwaway that pesky
								// metadata.
								that.isolate->DiscardThreadSpecificMetadata();
							}
							if (that.hit_memory_limit) {
								// This will unlock v8 and dispose, below
								break;
							}
							UnrefUv();
							return;
						}
					}

					// Execute handle tasks
					while (!handle_tasks.empty()) {
						auto task = std::move(handle_tasks.front());
						(*task)();
						handle_tasks.pop();
					}

					// Execute interrupt tasks
					while (!interrupt_tasks.empty()) {
						auto task = std::move(interrupt_tasks.front());
						(*task)();
						interrupt_tasks.pop();
					}

					// Execute regular tasks
					while (!tasks.empty()) {
						auto task = std::move(tasks.front());
						(*task)();
						tasks.pop();
					}
				}
			}
			// If we got here it means we're supposed to throw away this isolate due to an OOM memory
			// error
			assert(that.hit_memory_limit);
			exec_lock.unlock();
			that.Dispose();
			UnrefUv();
		}

		/**
		 * This is called in response to RequestInterrupt. In this case the isolate will already be
		 * locked and enterred.
		 */
		static void InterruptEntry(Isolate* isolate, void* param) {
			ShareableIsolate& that = *static_cast<ShareableIsolate*>(param);
			assert(that.status == Status::Running);
			decltype(interrupt_tasks) interrupt_tasks_copy;
			{
				std::unique_lock<std::mutex> lock(that.queue_mutex);
				std::swap(that.interrupt_tasks, interrupt_tasks_copy);
			}
			while (!interrupt_tasks_copy.empty()) {
				auto task = std::move(interrupt_tasks_copy.front());
				(*task)();
				interrupt_tasks_copy.pop();
			}
		}

		/**
		 * Helper around v8::Locker which does extra bookkeeping for us.
		 * - Updates `current`
		 * - Runs scheduled work
		 * - Also sets up handle scope
		 * - Throws exceptions from inner isolate into the outer isolate
		 */
		template <typename F, typename ...Args>
		auto Locker(F fn, Args&&... args) -> decltype(fn(args...)) {
			if (std::this_thread::get_id() != default_thread) {
				throw js_generic_error(
					"Calling a synchronous isolated-vm function from within an asynchronous isolated-vm function is not allowed."
				);
			}
			std::shared_ptr<ExternalCopy> error;
			{
				{
					std::unique_lock<std::mutex> lock(queue_mutex);
					if (life_cycle != LifeCycle::Normal || hit_memory_limit) {
						// nb: v8 lock is never set up
						lock.unlock();
						throw js_generic_error("Isolate is disposed or disposing");
					}
				}
				std::unique_lock<std::mutex> lock(exec_mutex);
				LockerHelper locker(*this);
				TryCatch try_catch(isolate);
				ExecuteHandleTasks();
				try {
					return TaskWrapper(fn(std::forward<Args>(args)...));
				} catch (const js_error_base& cc_error) {
					// memory errors will be handled below
					if (!hit_memory_limit) {
						// `stack` getter on Error needs a Context..
						assert(try_catch.HasCaught());
						Context::Scope context_scope(DefaultContext());
						error = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
					}
				}
			}
			// If we get here we can assume an exception was thrown
			ExecuteHandleTasks();
			if (error.get()) {
				(*current)->ThrowException(error->CopyInto());
				throw js_error_base();
			} else if (hit_memory_limit) {
				if (Isolate::GetCurrent() == isolate) {
					// Recursive call to ivm library? Just throw a C++ exception and leave the termination
					// exception as is
					throw js_error_base();
				} else if (Locker::IsLocked(isolate)) {
					// Recursive call but there is another isolate in between, this gets thrown to the middle
					// isolate
					throw js_generic_error("Isolate has exhausted v8 heap space.");
				} else {
					// We need to get rid of this isolate before it takes down the whole process
					{
						std::unique_lock<std::mutex> lock(queue_mutex);
						if (life_cycle == LifeCycle::Normal) {
							lock.unlock();
							Dispose();
						}
					}
					throw js_generic_error("Isolate has exhausted v8 heap space.");
				}
			} else {
				throw js_generic_error("An exception was thrown. Sorry I don't know more.");
			}
		}

		Local<Context> DefaultContext() const {
			return Local<Context>::New(isolate, default_context);
		}

		void ContextCreated(Local<Context> context) {
			std::string name = "<isolated-vm>";
			inspector_client->inspector->contextCreated(v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView((const uint8_t*)name.c_str(), name.length())));
		}

		void ContextDestroyed(Local<Context> context) {
			if (!root) { // TODO: This gets called for the root context because of ShareableContext's dtor :(
				inspector_client->inspector->contextDestroyed(context);
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
 * Most operations in this library can be decomposed into three phases.
 *
 * - Phase 1 [Isolate 1]: copy data out of current isolate
 * - Phase 2 [Isolate 2]: copy data into new isolate, run work, copy data out of isolate
 * - Phase 3 [Isolate 1]: copy results from phase 2 into the original isolate
 *
 * This class handles the locking and thread synchronization for either synchronous or
 * asynchronous functions. That way the same code can be used for both versions of each function.
 *
 * When `async=true` a promise is return which will be resolved after all the work is done
 * When `async=false` this will run in the calling thread until completion
 */
class ThreePhaseTask {
	public:
		ThreePhaseTask() = default;
		ThreePhaseTask(const ThreePhaseTask&) = delete;
		ThreePhaseTask& operator= (const ThreePhaseTask&) = delete;
		virtual ~ThreePhaseTask() = default;

		virtual void Phase2() = 0;
		virtual v8::Local<v8::Value> Phase3() = 0;

		template <bool async, typename T, typename ...Args>
		static v8::Local<v8::Value> Run(ShareableIsolate& second_isolate, Args&&... args) {

			if (async) {
				// Build a promise for outer isolate
				ShareableIsolate& first_isolate = ShareableIsolate::GetCurrent();
				auto context_local = first_isolate->GetCurrentContext();
				auto context_persistent = std::make_shared<Persistent<Context>>(first_isolate, context_local);
				auto promise_local = Promise::Resolver::New(first_isolate);
				auto promise_persistent = std::make_shared<Persistent<Promise::Resolver>>(first_isolate, promise_local);
				auto first_isolate_ref = first_isolate.GetShared();
				auto second_isolate_ref = second_isolate.GetShared();
				TryCatch try_catch(first_isolate);
				try {
					// Start the task
					shared_ptr<ThreePhaseTask> self = std::make_shared<T>(std::forward<Args>(args)...);
					// And continue in the second isolate
					second_isolate.ScheduleTask([self, first_isolate_ref, second_isolate_ref, promise_persistent, context_persistent]() {
						TryCatch try_catch(Isolate::GetCurrent());
						ShareableIsolate& first_isolate = *first_isolate_ref;
						try {
							// Continue the task
							self->Phase2();
							second_isolate_ref->TaskWrapper(0); // TODO: this is filthy
							// Finish back in first isolate
							first_isolate.ScheduleTask([self, promise_persistent, context_persistent]() {
								Isolate* isolate = Isolate::GetCurrent();
								auto context_local = Local<Context>::New(isolate, *context_persistent);
								Context::Scope context_scope(context_local);
								auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
								TryCatch try_catch(isolate);
								try {
									// Final callback
									Unmaybe(promise_local->Resolve(context_local, self->Phase3()));
									isolate->RunMicrotasks();
								} catch (js_error_base& cc_error) {
									// An error was caught while running Phase3()
									if (ShareableIsolate::GetCurrent().IsNormalLifeCycle()) {
										assert(try_catch.HasCaught());
										// If Reject fails then I think that's bad..
										Unmaybe(promise_local->Reject(context_local, try_catch.Exception()));
										isolate->RunMicrotasks();
									}
								}
							});
						} catch (js_error_base& cc_error) {
							// An error was caught while running Phase2(). Or perhaps first_isolate.ScheduleTask() threw.
							try {
								shared_ptr<ExternalCopy> err;
								if (try_catch.HasCaught()) {
									// Graceful JS exception
									Context::Scope context_scope(second_isolate_ref->DefaultContext());
									err = shared_ptr<ExternalCopy>(ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception()));
								} else {
									// Isolate is probably in trouble
									assert(!second_isolate_ref->IsNormalLifeCycle());
									err = std::make_shared<ExternalCopyError>(ExternalCopyError::ErrorType::Error, "Isolate is disposed or disposing");
								}

								// Schedule a task to enter the first isolate so we can throw the error at the promise
								first_isolate.ScheduleTask([promise_persistent, context_persistent](shared_ptr<ExternalCopy> error) {
									// Revive our persistent handles
									Isolate* isolate = Isolate::GetCurrent();
									auto context_local = Local<Context>::New(isolate, *context_persistent);
									Context::Scope context_scope(context_local);
									auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
									Local<Value> rejection;
									if (error.get()) {
										rejection = error->CopyInto();
									} else {
										rejection = Exception::Error(v8_string("An exception was thrown. Sorry I don't know more."));
									}
									// If Reject fails then I think that's bad..
									Unmaybe(promise_local->Reject(context_local, rejection));
									isolate->RunMicrotasks();
								}, std::move(err));
							} catch (js_error_base& cc_error2) {
								// If we get here it means first_isolate was disposed before we could return the result
								// from second_isolate back to it. There's not much we can do besides just forget about
								// it.
							}
						}
					});
				} catch (js_error_base& cc_error) {
					// An error was caught while running ctor (phase 1)
					assert(try_catch.HasCaught());
					Maybe<bool> ret = promise_local->Reject(context_local, try_catch.Exception());
					try_catch.Reset();
					Unmaybe(ret);
				}
				return promise_local->GetPromise();
			} else {
				// The sync case is a lot simpler, most of the work is done in second_isolate.Locker()
				unique_ptr<ThreePhaseTask> self = std::make_unique<T>(std::forward<Args>(args)...);
				if (&ShareableIsolate::GetCurrent() == &second_isolate) {
					// Shortcut when calling a sync method belonging to the currently entered isolate. This isn't an
					// optimization, it's used to bypass the deadlock prevention check in Locker()
					self->Phase2();
				} else {
					second_isolate.Locker([&self]() {
						self->Phase2();
						return 0;
					});
				}
				return self->Phase3();
			}
		}
};


/**
 * Run some v8 thing with a timeout
 */
template <typename F>
Local<Value> RunWithTimeout(uint32_t timeout_ms, ShareableIsolate& isolate, F&& fn) {
	bool did_timeout = false, did_finish = false;
	MaybeLocal<Value> result;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, [&did_timeout, &did_finish, &isolate]() {
				did_timeout = true;
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
	if (did_timeout) {
		if (isolate.IsNormalLifeCycle()) {
			isolate->CancelTerminateExecution();
		}
		throw js_generic_error("Script execution timed out.");
	} else if (isolate.DidHitMemoryLimit()) {
		// TODO: Consider finding a way to do this without allocating in the dangerous isolate
		throw js_generic_error("Isolate has exhausted v8 heap space.");
	}
	return Unmaybe(result);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winstantiation-after-specialization"
// These instantiations make msvc correctly link the template specializations in shareable_isolate.cc, but clang whines about it
template <>
MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const;
template MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const;

template <>
void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);
template void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);
#pragma clang diagnostic pop

}
