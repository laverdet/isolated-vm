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
template <class T, void (*D)(Persistent<T>&, ShareableIsolate*) = nullptr>
class ShareablePersistent {
	private:
		std::shared_ptr<ShareableIsolate> isolate;
		std::unique_ptr<Persistent<T>> handle;

	public:
		ShareablePersistent(Local<T> handle) :
			isolate(ShareableIsolate::GetCurrent().GetShared()),
			handle(std::make_unique<Persistent<T>>(*isolate, handle)) {
		}

		~ShareablePersistent() {
			struct DisposalTask : public Runnable {
				shared_ptr<ShareableIsolate> isolate;
				unique_ptr<Persistent<T>> handle;

				DisposalTask(shared_ptr<ShareableIsolate> isolate, unique_ptr<Persistent<T>> handle) : isolate(isolate), handle(std::move(handle)) {}

				void Run() {
					if (D != nullptr) {
						D(*handle, isolate.get());
					}
					handle->Reset();
				}
			};
			isolate->ScheduleTask(std::make_unique<DisposalTask>(isolate, std::move(handle)), true, false);
		}

		/**
		 * Dereference this persistent into local scope. This is only valid while the owned isolate is
		 * locked.
		 */
		Local<T> Deref() const {
			return Local<T>::New(*isolate, *handle);
		}

		/**
		 * Return underlying ShareableIsolate
		 */
		ShareableIsolate& GetIsolate() const {
			return *isolate;
		}
};

}
