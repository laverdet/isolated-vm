#pragma once
#include <node.h>
#include "shareable_isolate.h"

#include <memory>

namespace ivm {
using namespace v8;

/**
 * Wrapper for a persistent reference and the isolate that owns it. We can then wrap this again
 * in `std` memory managers to share amongst other isolates.
 */
template <class T>
class ShareablePersistent {
	private:
		std::shared_ptr<ShareableIsolate> isolate;
		Persistent<T> handle;

	public:
		ShareablePersistent(Local<T> handle) :
			isolate(ShareableIsolate::GetCurrent().GetShared()),
			handle(*isolate, handle) {
		}

		/**
		 * Dereference this persistent into local scope. This is only valid while the owned isolate is
		 * locked.
		 */
		Local<T> Deref() const {
			return Local<T>::New(*isolate, handle);
		}

		/**
		 * Operators which will return the underlying Persistent<> handle
		 */
		Persistent<T>& operator*() const {
			return handle;
		}

		Persistent<T>* operator->() const {
			return handle;
		}

		/**
		 * Return underlying ShareableIsolate
		 */
		ShareableIsolate& GetIsolate() const {
			return *isolate;
		}
};

}
