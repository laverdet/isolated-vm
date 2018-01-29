#pragma once
#include "runnable.h"
#include <memory>

namespace ivm {

class ShareableIsolate;
class IsolateHolder {
	friend class ShareableIsolate;
	private:
		std::shared_ptr<ShareableIsolate> isolate;

	public:
		IsolateHolder(std::shared_ptr<ShareableIsolate> isolate);
		IsolateHolder(const IsolateHolder&) = delete;
		IsolateHolder& operator= (const IsolateHolder&) = delete;
		~IsolateHolder() = default;
		void Dispose();
		std::shared_ptr<ShareableIsolate> GetIsolate();
		void ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate);
};

} // namespace ivm
