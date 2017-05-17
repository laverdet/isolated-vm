#pragma once
#include <node.h>
#include <assert.h>
#include "external_copy.h"
#include "util.h"

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

		Isolate* isolate;
		Persistent<Context> default_context;
		unique_ptr<ArrayBuffer::Allocator> allocator_ptr;
		std::mutex exec_mutex; // mutex for execution
		std::mutex queue_mutex; // mutex for queueing work
		std::queue<unique_ptr<std::function<void()>>> work;
		std::vector<unique_ptr<Persistent<Value>>> specifics;
		std::vector<unique_ptr<Persistent<FunctionTemplate>>> specifics_ft;
		std::map<Persistent<Object>*, std::pair<void(*)(void*), void*>> weak_persistents;

		enum class LifeCycle { Normal, Disposing, Disposed };
		LifeCycle life_cycle;
		bool root;

		/**
		 * Wrapper around Locker that sets our own `current`
		 */
		class LockerHelper {
			private:
				ShareableIsolate* last;
				Locker locker;

			public:
				LockerHelper(ShareableIsolate& isolate) : last(current), locker(isolate) {
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
			root(true) {
			assert(current == nullptr);
			current = this;
		}

		/**
		 * Create a new wrapped Isolate
		 */
		template <typename... Args>
		ShareableIsolate(unique_ptr<ArrayBuffer::Allocator> allocator_ptr, Args... args) :
			isolate(Isolate::New(args...)),
			allocator_ptr(std::move(allocator_ptr)),
			life_cycle(LifeCycle::Normal),
			root(false) {
			// Create a default context for the library to use if needed
			{
				v8::Locker locker(isolate);
				HandleScope handle_scope(isolate);
				default_context.Reset(isolate, Context::New(isolate));
			}
			// There will be no asynchronous Isolate ctor so we should throw away thread specifics in case
			// the client always uses async methods
			isolate->DiscardThreadSpecificMetadata();
		}

		~ShareableIsolate() {
			if (!root && life_cycle == LifeCycle::Normal) {
				isolate->Dispose();
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
			std::unique_lock<std::mutex> lock(exec_mutex);
			{
				std::unique_lock<std::mutex> lock2(queue_mutex);
				if (life_cycle != LifeCycle::Normal) {
					throw js_generic_error("Isolate is already disposed or disposing");
				}
				life_cycle = LifeCycle::Disposing;
			}
			{
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
		}

		/**
		 * Schedules a task to run when this isolate is locked. It may run immediately or it may run
		 * later. If you throw an exception the process will burn down.
		 */
		template <typename F>
		auto ScheduleWork(F&& fn) -> decltype(fn()) {
			auto fn_ptr = std::make_unique<std::function<void()>>(std::forward<F>(fn));
			if (Isolate::GetCurrent() == isolate) {
				(*fn_ptr)();
			} else {
				std::unique_lock<std::mutex> lock(queue_mutex);
				this->work.push(std::move(fn_ptr));
			}
		}

		void ExecuteWork() {
			while (true) {
				std::queue<unique_ptr<std::function<void()>>> tasks;
				{
					std::unique_lock<std::mutex> lock(queue_mutex);
					std::swap(tasks, work);
				}
				if (tasks.empty()) {
					return;
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
		template <typename F>
		static auto Locker(ShareableIsolate& isolate, F fn) -> decltype(fn()) {
			std::shared_ptr<ExternalCopy> error;
			{
				std::unique_lock<std::mutex> lock(isolate.exec_mutex);
				if (isolate.life_cycle != LifeCycle::Normal) {
					// nb: v8 lock is never set up
					throw js_generic_error("Isolate is disposed or disposing");
				}
				LockerHelper locker(isolate);
				Isolate::Scope isolate_scope(isolate);
				HandleScope handle_scope(isolate);
				isolate.ExecuteWork();
				TryCatch try_catch(isolate);
				try {
					return fn();
				} catch (const js_error_base& cc_error) {
					assert(try_catch.HasCaught());
					// `stack` getter on Error needs a Context..
					Context::Scope context_scope(isolate.DefaultContext());
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

		Local<Context> DefaultContext() {
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

template <>
MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const;

template <>
void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle);

}
