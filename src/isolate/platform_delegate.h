#pragma once
#include "lib/lockable.h"
#include "node_wrapper.h"
#include "v8_version.h"
#include <v8-platform.h>
#include <mutex>
#include <unordered_map>

#if NODE_MODULE_VERSION < 81
#define IVM_USE_PLATFORM_DELEGATE_HACK 1
#endif

#if IVM_USE_PLATFORM_DELEGATE_HACK
namespace v8 {
namespace internal {
class V8 {
	public:
		static auto GetCurrentPlatform() -> v8::Platform*; // naughty
};
} // namespace internal
} // namespace v8
#endif

namespace ivm {

#if NODE_MODULE_VERSION < 81
class IsolatePlatformDelegate {
	public:
		virtual auto GetForegroundTaskRunner() -> std::shared_ptr<v8::TaskRunner> = 0;
		virtual auto IdleTasksEnabled() -> bool = 0;
};
#else
using IsolatePlatformDelegate = node::IsolatePlatformDelegate;
#endif

// Normalize this interface from v8
class TaskRunner : public v8::TaskRunner {
	public:
		// Methods for v8::TaskRunner
		void PostTask(std::unique_ptr<v8::Task> task) override = 0;
		void PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) override = 0;
		void PostIdleTask(std::unique_ptr<v8::IdleTask> /*task*/) final { std::terminate(); }
		// Can't be final because symbol is also used in IsolatePlatformDelegate
		auto IdleTasksEnabled() -> bool override { return false; };

#if V8_AT_LEAST(7, 1, 316)
		// Added in e8faae72
		auto NonNestableTasksEnabled() const -> bool final { return true; }
#else
		virtual void PostNonNestableTask(std::unique_ptr<v8::Task> task) = 0;
#endif

#if V8_AT_LEAST(7, 4, 197)
		// Added in d342122f
		void PostNonNestableDelayedTask(std::unique_ptr<v8::Task> /*task*/, double /*delay_in_seconds*/) final { std::terminate(); }
		auto NonNestableDelayedTasksEnabled() const -> bool final { return false; }
#endif
};

#if IVM_USE_PLATFORM_DELEGATE_HACK
class PlatformDelegate : public v8::Platform {
#else
class PlatformDelegate {
#endif

	public:
		PlatformDelegate() = default;
		explicit PlatformDelegate(node::MultiIsolatePlatform* node_platform) : node_platform{node_platform} {}
		PlatformDelegate(const PlatformDelegate&) = delete;
		PlatformDelegate(PlatformDelegate&&) = delete;
		~PlatformDelegate() = default; // NOLINT(modernize-use-override) -- this only sometimes inherits from v8::Platform
		auto operator=(const PlatformDelegate&) = delete;
		auto operator=(PlatformDelegate&& delegate) noexcept -> PlatformDelegate& {
			node_platform = std::exchange(delegate.node_platform, nullptr);
#if IVM_USE_PLATFORM_DELEGATE_HACK
			*isolate_map.write() = std::move(*delegate.isolate_map.write());
#endif
			return *this;
		}

		static void InitializeDelegate();
		static void RegisterIsolate(v8::Isolate* isolate, IsolatePlatformDelegate* isolate_delegate);
		static void UnregisterIsolate(v8::Isolate* isolate);

#if IVM_USE_PLATFORM_DELEGATE_HACK
		/**
		 * ~ An Abridged History of v8's threading and task API ~
		 *
		 * 6.4.168:
		 * - `GetForegroundTaskRunner(Isolate*)` and `GetBackgroundTaskRunner(Isolate*)` are introduced [c690f54d]
		 *
		 * 6.7.1:
		 * - `GetBackgroundTaskRunner` renamed to `GetWorkerThreadsTaskRunner` (old function is
		 *   deprecated) [70222a9d]
		 * - `CallOnBackgroundThread(Task*, ExpectedRuntime)` unofficially deprecated in favor of new
		 *   `CallOnWorkerThread(Task* task)` [86b4b534]
		 * - `CallBlockingTaskFromBackgroundThread(Task* task)` added [2600038d]
		 *
		 * 6.7.179:
		 * - `CallOnWorkerThread` and `CallBlockingTaskOnWorkerThread` parameters both
		 *   changed to `(std::unique_ptr<Task> task)` [1983f305]
		 *
		 * 6.8.117:
		 * - `GetWorkerThreadsTaskRunner` removed [4ac96190]
		 * - `CallDelayedOnWorkerThread(std::unique_ptr<Task> task, double delay_in_seconds)` added [4b13a22f]
		 *
		 * 6.8.242:
		 * - `CallOnBackgroundThread`, `GetBackgroundTaskRunner`, `NumberOfAvailableBackgroundThreads`
		 *   removed [8f6ffbfc]
		 * - `CallDelayedOnWorkerThread`, `CallOnWorkerThread`, `GetForegroundTaskRunner`,
		 *   `NumberOfWorkerThreads` becomes pure virtual [8f6ffbfc]
		 *
		 * 7.1.263:
		 * - `CallOnForegroundThread`, `CallDelayedOnForegroundThread`, and `CallIdleOnForegroundThread`
		 *   deprecated
		 *
		 * 7.1.316:
		 * - `PostNonNestableTask(std::unique_ptr<Task> task)` and `NonNestableTasksEnabled()` added to
		 *   `v8::TaskRunner`
		 *
		 *
		 * ~ Meanwhile in nodejs ~
		 *
		 * 10.0.0 (module version 64):
		 * - Updates v8 to 6.6.346 but includes random internal changes from v8 6.7.x [2a3f8c3a]
		 *
		 * 10.9.0 (module version 64):
		 * - Updates v8 to 6.8.275 but continues to include old API requirements from v8 v6.7.x [5fa3ffad]
		 *
		 * 11.2.0 (module version 67):
		 * - Updates v8 to 7.0.276.38
		 *
		 */
		auto NumberOfWorkerThreads() -> int final {
			return node_platform->NumberOfWorkerThreads();
		}

#if V8_AT_LEAST(6, 8, 242)
		void CallOnWorkerThread(std::unique_ptr<v8::Task> task) final {
			node_platform->CallOnWorkerThread(std::move(task));
		}
#else
		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime /*expected_runtime*/) final {
			node_platform->CallOnWorkerThread(std::unique_ptr<v8::Task>(task));
		}
#endif

#if !NODE_MODULE_OR_V8_AT_LEAST(67, 6, 7, 1)
		std::shared_ptr<v8::TaskRunner> GetBackgroundTaskRunner(v8::Isolate* isolate) final {
			return node_platform->GetBackgroundTaskRunner(isolate);
		}
#endif

		auto GetForegroundTaskRunner(v8::Isolate* isolate) -> std::shared_ptr<v8::TaskRunner> final {
			auto lock = isolate_map.read();
			auto ii = lock->find(isolate);
			if (ii == lock->end()) {
				return node_platform->GetForegroundTaskRunner(isolate);
			} else {
				return ii->second->GetForegroundTaskRunner();
			}
		}

		void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) final {
			GetForegroundTaskRunner(isolate)->PostTask(std::unique_ptr<v8::Task>{task});
		}

		void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task, double delay_in_seconds) final {
			GetForegroundTaskRunner(isolate)->PostDelayedTask(std::unique_ptr<v8::Task>{task}, delay_in_seconds);
		}

#if NODE_MODULE_OR_V8_AT_LEAST(67, 6, 8, 242)
		void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task, double delay_in_seconds) final {
			node_platform->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds);
		}
#elif NODE_MODULE_OR_V8_AT_LEAST(67, 6, 8, 117)
		void CallDelayedOnWorkerThread(std::unique_ptr<v8::Task> task, double delay_in_seconds) {
			node_platform->CallDelayedOnWorkerThread(std::move(task), delay_in_seconds);
		}
#endif

		void CallIdleOnForegroundThread(v8::Isolate* isolate, v8::IdleTask* task) final {
			node_platform->CallIdleOnForegroundThread(isolate, task);
		}

		auto IdleTasksEnabled(v8::Isolate* isolate) -> bool final {
			return GetForegroundTaskRunner(isolate)->IdleTasksEnabled();
		}

		// 11ba497c made this implementation required, 837b8016 added `SystemClockTimeMillis()`
		auto CurrentClockTimeMillis() -> double final {
			return node_platform->CurrentClockTimeMillis();
		}

		auto MonotonicallyIncreasingTime() -> double final {
			return node_platform->MonotonicallyIncreasingTime();
		}

		auto GetTracingController() -> v8::TracingController* final {
			return node_platform->GetTracingController();
		}

	private:
		lockable_t<std::unordered_map<v8::Isolate*, IsolatePlatformDelegate*>> isolate_map;
#endif


		node::MultiIsolatePlatform* node_platform = nullptr;
};

} // namespace ivm
