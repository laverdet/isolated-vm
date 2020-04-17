#pragma once
#include "platform_delegate.h"
#include "runnable.h"
#include "v8_version.h"
#include "lib/lockable.h"
#include <v8-platform.h>
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
		auto GetIsolate() -> std::shared_ptr<IsolateEnvironment>;
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

// This needs to be separate from IsolateHolder because v8 holds references to this indefinitely and
// we don't want it keeping the isolate alive.
class IsolateTaskRunner final : public TaskRunner {
	public:
		explicit IsolateTaskRunner(const std::shared_ptr<IsolateEnvironment>& isolate) : weak_env{isolate} {}
		IsolateTaskRunner(const IsolateTaskRunner&) = delete;
		~IsolateTaskRunner() final = default;
		auto operator=(const IsolateTaskRunner&) = delete;

		// Methods for v8::TaskRunner
		void PostTask(std::unique_ptr<v8::Task> task) final;
		void PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) final;
		void PostNonNestableTask(std::unique_ptr<v8::Task> task) final { PostTask(std::move(task)); }

	private:
		std::weak_ptr<IsolateEnvironment> weak_env;
};

} // namespace ivm
