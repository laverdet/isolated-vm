#pragma once
#include <node.h>
#include "holder.h"
#include "environment.h"
#include "util.h"
#include <memory>

namespace ivm {

/**
 * Wrapper for a persistent reference and the isolate that owns it. We can then wrap this again
 * in `std` memory managers to share amongst other isolates.
 */
template <class T>
class ShareablePersistent {
	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::unique_ptr<Persistent<T>> handle;

	public:
		ShareablePersistent(v8::Local<T> handle) :
			isolate(IsolateEnvironment::GetCurrentHolder()),
			handle(std::make_unique<Persistent<T>>(v8::Isolate::GetCurrent(), handle)) {
		}

		~ShareablePersistent() {
			struct DisposalTask : public Runnable {
				std::unique_ptr<Persistent<T>> handle;

				DisposalTask(std::unique_ptr<v8::Persistent<T>> handle) : handle(std::move(handle)) {}

				void Run() {
					// Probably not even needed since the dtor will do this for us
					handle->Reset();
				}
			};
			isolate->ScheduleTask(std::make_unique<DisposalTask>(std::move(handle)), true, false);
		}

		/**
		 * Dereference this persistent into local scope. This is only valid while the owned isolate is
		 * locked.
		 */
		v8::Local<T> Deref() const {
			return v8::Local<T>::New(v8::Isolate::GetCurrent(), *handle);
		}

		/**
		 * Return underlying IsolateEnvironment
		 */
		std::shared_ptr<IsolateHolder> GetIsolateHolder() const {
			return isolate;
		}
};

} // namespace ivm
