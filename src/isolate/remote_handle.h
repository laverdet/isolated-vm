#pragma once
#include <v8.h>
#include "environment.h"
#include "holder.h"
#include <memory>

namespace ivm {

template <typename T>
class RemoteHandle {
	public:
		using Traits = v8::NonCopyablePersistentTraits<T>;
		using HandleType = v8::Persistent<T, Traits>;

	private:
		std::shared_ptr<IsolateHolder> isolate;
		std::unique_ptr<HandleType> handle;

	public:
		RemoteHandle(v8::Local<T> handle) :
			isolate(IsolateEnvironment::GetCurrentHolder()),
			handle(std::make_unique<HandleType>(v8::Isolate::GetCurrent(), handle))
		{
			static_assert(!Traits::kResetInDestructor, "Do not reset in destructor");
		}

		~RemoteHandle() {
			struct DisposalTask : public Runnable {
				std::unique_ptr<HandleType> handle;
				DisposalTask(std::unique_ptr<HandleType> handle) : handle(std::move(handle)) {}

				void Run() {
					handle->Reset();
				}
			};
			isolate->ScheduleTask(std::make_unique<DisposalTask>(std::move(handle)), true, false);
		}

		RemoteHandle(const RemoteHandle&) = delete;
		RemoteHandle& operator= (const RemoteHandle&) = delete;

		v8::Local<T> Deref() const {
			return v8::Local<T>::New(v8::Isolate::GetCurrent(), *handle);
		}

		void Reset() {
			handle->Reset();
		}
};

} // namespace ivm
