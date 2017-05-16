#pragma once
#include <node.h>
#include "shareable_isolate.h"

#include <memory>

namespace ivm {
using namespace v8;

/**
 * Wrapper for a persistent reference and the isolate that owns it. We can then wrap this again
 * in `std` memory managers to share amongst other isolates. When the destructor for this is called
 * (i.e. when all shared_ptrs<> to it are gone) it will schedule a release of the real Persistent<>
 * handle in the isolate to which it belongs.
 */
template <class T>
class ShareablePersistent {
	private:
		std::shared_ptr<ShareableIsolate> isolate;
		// This is a unique_ptr<> because the Persistent will slightly outlive the ShareablePersistent,
		// depending on when we can next lock the isolate
		std::unique_ptr<Persistent<T>> handle;

	public:
		ShareablePersistent(Local<T> handle) :
			isolate(ShareableIsolate::GetCurrent().GetShared()),
			handle(new Persistent<T>(*isolate, handle)) {
		}

		~ShareablePersistent() {
			Persistent<T>* handle = this->handle.release();
			isolate->ScheduleWork([ handle ]() {
				handle->Reset();
				delete handle;
			});
		}

		/**
		 * Dereference this persistent into local scope. This is only valid while the owned isolate is
		 * locked.
		 */
		Local<T> Deref() const {
			return Local<T>::New(*isolate, *handle);
		}

		/**
		 * Operators which will return the underlying Persistent<> handle
		 */
		Persistent<T>& operator*() const {
			return *handle;
		}

		Persistent<T>* operator->() const {
			return handle.get();
		}

		/**
		 * Return underlying ShareableIsolate
		 */
		ShareableIsolate& GetIsolate() const {
			return *isolate;
		}
};

}
