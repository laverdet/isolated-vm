#pragma once
#include "runnable.h"
#include <condition_variable>
#include <mutex>
#include <memory>

namespace ivm {

class IsolateEnvironment;
class IsolateHolder {
	friend class IsolateEnvironment;
	private:
		std::shared_ptr<IsolateEnvironment> isolate;
		std::condition_variable cv;
		std::mutex mutex;
		bool is_disposed = false;

	public:
		explicit IsolateHolder(std::shared_ptr<IsolateEnvironment> isolate);
		IsolateHolder(const IsolateHolder&) = delete;
		IsolateHolder& operator= (const IsolateHolder&) = delete;
		~IsolateHolder() = default;
		void Dispose();
		void ReleaseAndJoin();
		std::shared_ptr<IsolateEnvironment> GetIsolate();
		void ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate, bool handle_task = false);
};

} // namespace ivm
