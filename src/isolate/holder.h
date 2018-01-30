#pragma once
#include "runnable.h"
#include <memory>

namespace ivm {

class IsolateEnvironment;
class IsolateHolder {
	friend class IsolateEnvironment;
	private:
		std::shared_ptr<IsolateEnvironment> isolate;

	public:
		IsolateHolder(std::shared_ptr<IsolateEnvironment> isolate);
		IsolateHolder(const IsolateHolder&) = delete;
		IsolateHolder& operator= (const IsolateHolder&) = delete;
		~IsolateHolder() = default;
		void Dispose();
		std::shared_ptr<IsolateEnvironment> GetIsolate();
		void ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate);
};

} // namespace ivm
