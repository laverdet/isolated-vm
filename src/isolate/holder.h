#pragma once
#include "runnable.h"
#include "../lib/lockable.h"
#include <condition_variable>
#include <mutex>
#include <memory>

namespace ivm {

class IsolateEnvironment;
class IsolateHolder {
	friend IsolateEnvironment;

	public:
		explicit IsolateHolder(std::shared_ptr<IsolateEnvironment> isolate) : state{std::move(isolate)} {}
		IsolateHolder(const IsolateHolder&) = delete;
		~IsolateHolder() = default;
		auto operator=(const IsolateHolder&) = delete;

		auto Dispose() -> bool;
		void ReleaseAndJoin();
		std::shared_ptr<IsolateEnvironment> GetIsolate();
		void ScheduleTask(std::unique_ptr<Runnable> task, bool run_inline, bool wake_isolate, bool handle_task = false);

	private:
		struct State {
			explicit State(std::shared_ptr<IsolateEnvironment> isolate) : isolate{std::move(isolate)} {}
			std::shared_ptr<IsolateEnvironment> isolate;
			bool is_disposed = false;
		};
		using LockableState = lockable_t<State>;
		LockableState state;
		LockableState::condition_variable_t cv;
};

} // namespace ivm
