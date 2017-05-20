#pragma once
#include <node.h>
#include <uv.h>
#include <assert.h>
#include "thread_pool.h"
#include "external_copy.h"
#include "util.h"

#include "apply_from_tuple.h"
#include <algorithm>
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
		static thread_local ShareableIsolate* current;
		static size_t specifics_count;
		static thread_pool_t thread_pool;
		static uv_async_t root_async;
		static std::thread::id default_thread;
		static int uv_refs;

		Isolate* isolate;
		Persistent<Context> default_context;
		unique_ptr<ArrayBuffer::Allocator> allocator_ptr;
		std::mutex exec_mutex; // mutex for execution
		std::mutex queue_mutex; // mutex for queueing work
		std::queue<unique_ptr<std::function<void()>>> tasks;
		std::vector<unique_ptr<Persistent<Value>>> specifics;
		std::vector<unique_ptr<Persistent<FunctionTemplate>>> specifics_ft;
		std::map<Persistent<Object>*, std::pair<void(*)(void*), void*>> weak_persistents;
		thread_pool_t::affinity_t thread_affinity;

		enum class LifeCycle { Normal, Disposing, Disposed };
		LifeCycle life_cycle;

		enum class Status { Waiting, Running };
		Status status;

		bool root;

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

	public:
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

	public:
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
			root(true) {
			assert(current == nullptr);
			current = this;
			uv_async_init(uv_default_loop(), &root_async, WorkerEntryRoot);
			uv_unref((uv_handle_t*)&root_async);
			default_thread = std::this_thread::get_id();
		}

		/**
		 * Create a new wrapped Isolate
		 */
		template <typename... Args>
		ShareableIsolate(unique_ptr<ArrayBuffer::Allocator> allocator_ptr, Args... args) :
			isolate(Isolate::New(args...)),
			allocator_ptr(std::move(allocator_ptr)),
			life_cycle(LifeCycle::Normal),
			status(Status::Waiting),
			root(false) {
			// Create a default context for the library to use if needed
			{
				v8::Locker locker(isolate);
				HandleScope handle_scope(isolate);
				default_context.Reset(isolate, Context::New(isolate));
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
		}

		/**
		 * Convenience operators to work with underlying isolate
		 */
		operator Isolate*() const {
			assert(life_cycle != LifeCycle::Disposed);
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
		 * Dispose of an isolate and clean up dangling handles
		 */
		void Dispose() {
			if (root) {
				throw js_generic_error("Cannot dispose root isolate");
			}
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				if (life_cycle != LifeCycle::Normal) {
					throw js_generic_error("Isolate is already disposed or disposing");
				}
				life_cycle = LifeCycle::Disposing;
			}
			std::unique_lock<std::mutex> lock(exec_mutex);
			{
				assert(tasks.empty());
				LockerHelper locker(*this);
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
			assert(tasks.empty());
		}

		/**
		 * Schedule complex work to run when this isolate is locked. If the isolate is already locked
		 * the task will be run before unlocking, but after the current tasks have finished.
		 */
		template <typename F, typename ...Args>
		void ScheduleTask(F&& fn, Args&&... args) {
			auto fn_ptr = std::make_unique<std::function<void()>>(std::bind(std::forward<F>(fn), std::forward<Args>(args)...));
			std::unique_lock<std::mutex> lock(queue_mutex);
			if (life_cycle != LifeCycle::Normal) {
				throw js_generic_error("Isolate is disposed or disposing");
			}
			this->tasks.push(std::move(fn_ptr));
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
			}
		}

		static void WorkerEntryRoot(uv_async_s* async) {
			if (async->data != nullptr) { // see WorkerEntry nullptr async message
				WorkerEntry(true, async->data);
			}
		}

		static void WorkerEntry(bool pool_thread, void* param) {
			ShareableIsolate& that = *static_cast<ShareableIsolate*>(param);
			assert(that.status == Status::Running);
			std::unique_lock<std::mutex> exec_lock(that.exec_mutex);
			LockerHelper locker(that);
			while (true) {
				std::queue<unique_ptr<std::function<void()>>> tasks;
				{
					std::unique_lock<std::mutex> lock(that.queue_mutex);
					std::swap(tasks, that.tasks);
					if (tasks.empty()) {
						that.status = Status::Waiting;
						if (!pool_thread) {
							// In this case the thread pool was full so this loop was run in a temporary thread
							// that will be thrown away after this function finishes. Throwaway that pesky
							// metadata.
							that.isolate->DiscardThreadSpecificMetadata();
						}
						if (--uv_refs == 0) {
							uv_unref((uv_handle_t*)&root_async);
							// If we are the last ones to unref then node doesn't exit unless someone else
							// unrefs?? This seems like an obvious libuv bug but I don't want to investigate.
							root_async.data = nullptr;
							uv_async_send(&root_async);
						}
						return;
					}
				}
				do {
					auto task = std::move(tasks.front());
					(*task)();
					tasks.pop();
				} while (!tasks.empty());
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
					if (life_cycle != LifeCycle::Normal) {
						// nb: v8 lock is never set up
						throw js_generic_error("Isolate is disposed or disposing");
					}
				}
				std::unique_lock<std::mutex> lock(exec_mutex);
				LockerHelper locker(*this);
				TryCatch try_catch(isolate);
				try {
					return fn(std::forward<Args>(args)...);
				} catch (const js_error_base& cc_error) {
					assert(try_catch.HasCaught());
					// `stack` getter on Error needs a Context..
					Context::Scope context_scope(DefaultContext());
					error = ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception());
				}
			}
			// If we get here we can assume an exception was thrown
			if (error.get()) {
				(*current)->ThrowException(error->CopyInto());
				throw js_error_base();
			} else {
				throw js_generic_error("An exception was thrown. Sorry I don't know more.");
			}
		}

		Local<Context> DefaultContext() const {
			return Local<Context>::New(isolate, default_context);
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
 * This template handles the locking and thread synchronization for either synchronous or
 * asynchronous functions. That way the same code can be used for both versions of each function.
 *
 * When `async=true` a promise is return which will be resolved after all the work is done
 * When `async=false` this will run in the calling thread until completion
 */
template <bool async, typename F1, typename F2, typename F3>
v8::Local<v8::Value> ThreePhaseRunner(ShareableIsolate& second_isolate, F1 fn1, F2 fn2, F3 fn3) {
	if (async) {
		// Build a promise for outer isolate
		ShareableIsolate& first_isolate = ShareableIsolate::GetCurrent();
		auto context_local = first_isolate->GetCurrentContext();
		auto context_persistent = std::make_shared<Persistent<Context>>(first_isolate, context_local);
		auto promise_local = Promise::Resolver::New(first_isolate);
		auto promise_persistent = std::make_shared<Persistent<Promise::Resolver>>(first_isolate, promise_local);
		TryCatch try_catch(first_isolate);
		try {
			// Enter the second isolate with the results from fn1
			second_isolate.ScheduleTask([&first_isolate, &second_isolate, promise_persistent, context_persistent](decltype(fn1()) fn1_result, F2 fn2, F3 fn3) {
				TryCatch try_catch(Isolate::GetCurrent());
				try {
					// Schedule a task to enter the first isolate again with the results from fn2
					first_isolate.ScheduleTask([promise_persistent, context_persistent](decltype(apply_from_tuple(fn2, fn1_result)) fn2_result, F3 fn3) {
						Isolate* isolate = Isolate::GetCurrent();
						auto context_local = Local<Context>::New(isolate, *context_persistent);
						Context::Scope context_scope(context_local);
						auto promise_local = Local<Promise::Resolver>::New(isolate, *promise_persistent);
						TryCatch try_catch(isolate);
						try {
							Unmaybe(promise_local->Resolve(context_local, apply_from_tuple(fn3, std::move(fn2_result)))); // <-- fn3() is called here
						} catch (js_error_base& cc_error) {
							// An error was caught while running fn3()
							assert(try_catch.HasCaught());
							// If Reject fails then I think that's bad..
							Unmaybe(promise_local->Reject(context_local, try_catch.Exception()));
						}
					}, apply_from_tuple(fn2, std::move(fn1_result)), std::move(fn3)); // <-- fn2() is called here
				} catch (js_error_base& cc_error) {
					// An error was caught while running fn2()
					assert(try_catch.HasCaught());
					Context::Scope context_scope(second_isolate.DefaultContext());
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
					}, shared_ptr<ExternalCopy>(ExternalCopy::CopyIfPrimitiveOrError(try_catch.Exception())));
				}
			}, fn1(), std::move(fn2), std::move(fn3)); // <-- fn1() is called here
		} catch (js_error_base& cc_error) {
			// An error was caught while running fn1()
			assert(try_catch.HasCaught());
			promise_local->Reject(context_local, try_catch.Exception());
			try_catch.Reset();
		}
		return promise_local->GetPromise();
	} else {
		// The sync case is a lot simpler, most of the work is done in second_isolate.Locker()
		return apply_from_tuple(fn3, second_isolate.Locker([] (decltype(fn1()) fn1_result, F2 fn2) {
			return apply_from_tuple(fn2, fn1_result);
		}, fn1(), std::move(fn2)));
	}
}

template <>
MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const;

template <>
void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);

}
