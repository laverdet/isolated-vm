#pragma once
#ifdef _WIN32
#define ISOLATED_VM_MODULE extern "C" __declspec(dllexport)
#else
#define ISOLATED_VM_MODULE extern "C"
#endif

#include "../isolate/environment.h"
#include "../isolate/holder.h"
#include "../isolate/remote_handle.h"
#include "../isolate/runnable.h"
#include <memory>

namespace isolated_vm {
	using Runnable = ivm::Runnable;
	// ^ The only thing you need to know: `virtual void Run() = 0`

	class IsolateHolder {
		private:
			std::shared_ptr<ivm::IsolateHolder> holder;
			IsolateHolder(std::shared_ptr<ivm::IsolateHolder> holder) : holder(std::move(holder)) {
				ivm::IsolateEnvironment::Scheduler::IncrementUvRef();
			}

		public:
			IsolateHolder(const IsolateHolder& that) : holder(that.holder) {
				ivm::IsolateEnvironment::Scheduler::IncrementUvRef();
			}

			IsolateHolder(IsolateHolder&& that) : holder(std::move(that.holder)) {
				ivm::IsolateEnvironment::Scheduler::IncrementUvRef();
			}

			IsolateHolder& operator=(const IsolateHolder&) = default;
			IsolateHolder& operator=(IsolateHolder&) = default;

			~IsolateHolder() {
				ivm::IsolateEnvironment::Scheduler::DecrementUvRef();
			}

			static IsolateHolder GetCurrent() {
				return IsolateHolder(ivm::IsolateEnvironment::GetCurrentHolder());
			}

			void ScheduleTask(std::unique_ptr<Runnable> runnable) {
				holder->ScheduleTask(std::move(runnable), false, true, false);
			}

			void Release() {
				holder.reset();
			}
	};

	template <typename T>
	class RemoteHandle {
		private:
			std::shared_ptr<ivm::RemoteHandle<T>> handle;

		public:
			explicit RemoteHandle(v8::Local<T> handle) : handle(std::make_shared<ivm::RemoteHandle<T>>(handle)) {}

			auto operator*() const {
				return handle->Deref();
			}

			void Release() {
				handle.reset();
			}
	};
} // namespace isolated_vm
