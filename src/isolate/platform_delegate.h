#pragma once
#include <v8.h>
#include <v8-platform.h>
#include "environment.h"
#include "v8_version.h"
#include "../timer.h"

namespace v8 {
namespace internal {
class V8 {
	public:
		static v8::Platform* GetCurrentPlatform(); // naughty
};
} // namespace internal
} // namespace v8

namespace ivm {

class PlatformDelegate : public v8::Platform {
	private:
		v8::Isolate* node_isolate;
		v8::Platform* node_platform;
		v8::Isolate* tmp_isolate = nullptr;

		class TaskHolder : public Runnable {
			private:
				std::unique_ptr<v8::Task> task;
			public:
				explicit TaskHolder(v8::Task* task) : task(task) {}
				explicit TaskHolder(std::unique_ptr<v8::Task>&& task) : task(std::move(task)) {}
				explicit TaskHolder(TaskHolder&& task) : task(std::move(task.task)) {}
				void Run() final {
					task->Run();
				}
		};

	public:
		struct TmpIsolateScope {
			explicit TmpIsolateScope(v8::Isolate* isolate) {
				PlatformDelegate::DelegateInstance().tmp_isolate = isolate;
			}
			TmpIsolateScope(TmpIsolateScope&) = delete;
			TmpIsolateScope& operator=(const TmpIsolateScope&) = delete;

			~TmpIsolateScope() {
				PlatformDelegate::DelegateInstance().tmp_isolate = nullptr;
			}
		};

		PlatformDelegate(v8::Isolate* node_isolate, v8::Platform* node_platform) : node_isolate(node_isolate), node_platform(node_platform) {}

		static PlatformDelegate& DelegateInstance() {
			static PlatformDelegate delegate(v8::Isolate::GetCurrent(), v8::internal::V8::GetCurrentPlatform());
			return delegate;
		}

		static void InitializeDelegate() {
			PlatformDelegate& instance = DelegateInstance();
			v8::V8::ShutdownPlatform();
			v8::V8::InitializePlatform(&instance);
		}

		/**
		 * ~ An Abridged History of v8's threading and task API ~
		 *
		 * 6.4.168:
		 * - `GetForegroundTaskRunner(Isolate*)` and `GetBackgroundTaskRunner(Isolate*)` are introduced [c690f54d]
		 *
		 * 6.7.1:
		 * - `GetBackgroundTaskRunner` renamed to `GetWorkerThreadsTaskRunner` [70222a9d]
		 * - `CallOnWorkerThread(Task* task)` added [86b4b534]
		 * - `ExpectedRuntime` parameter removed from `CallOnBackgroundThread(Task*, ExpectedRuntime)` [86b4b534]
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
		 * - `CallOnBackgroundThread`, `NumberOfWorkerThreads`, `GetForegroundTaskRunner`, and
		 *   `CallOnWorkerThread` removed [8f6ffbfc]
		 * - `CallDelayedOnWorkerThread` becomse pure virtual [8f6ffbfc]
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
		 * 10.0.0:
		 * - Updates v8 to 6.6.346 but includes random internal changes from v8 6.7.x [2a3f8c3a]
		 *
		 * 10.9.0:
		 * - Updates v8 to 6.8.275 but continues to include old API requirements from v8 v6.7.x [5fa3ffad]
		 *
		 */

#if V8_AT_LEAST(6, 7, 1)
		int NumberOfWorkerThreads() final {
			return node_platform->NumberOfWorkerThreads();
		}
#endif

#if defined(NODE_MODULE_VERSION) ? NODE_MODULE_VERSION >= 64 : V8_AT_LEAST(6, 7, 179)
		// v8 commit 86b4b534 renamed this function and changed the signature. This made it to v8
		// v6.7.1, but 1983f305 further changed the signature.
		// These were both backported to nodejs in commmit 2a3f8c3a.
		void CallOnWorkerThread(std::unique_ptr<v8::Task> task) final {
			node_platform->CallOnWorkerThread(std::move(task));
		}

		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime /* expected_runtime */) final {
			// TODO: Properly count these tasks against isolate timers. How common are background tasks??
			node_platform->CallOnWorkerThread(std::unique_ptr<v8::Task>(task));
		}
#else
		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime expected_runtime) final {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
		}
#endif

#if V8_AT_LEAST(6, 4, 168)
		std::shared_ptr<v8::TaskRunner> GetBackgroundTaskRunner(v8::Isolate* /* isolate */) final {
			return node_platform->GetBackgroundTaskRunner(node_isolate);
		}

	private:
		// nb: The v8 documentation says that methods on this object may be called from any thread.
		class ForegroundTaskRunner : public v8::TaskRunner {
			private:
				std::weak_ptr<IsolateHolder> holder;

			public:
				ForegroundTaskRunner(std::shared_ptr<IsolateHolder> holder) : holder(std::move(holder)) {}

				void PostTask(std::unique_ptr<v8::Task> task) final {
					auto ref = holder.lock();
					if (ref) {
						ref->ScheduleTask(std::make_unique<TaskHolder>(std::move(task)), false, false, true);
					}
				}

				void PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) final {
					auto shared_task = std::make_shared<TaskHolder>(std::move(task));
					auto holder = this->holder;
					timer_t::wait_detached(delay_in_seconds * 1000, [holder, shared_task](void* next) {
						auto ref = holder.lock();
						if (ref) {
							// Don't wake the isolate because that will affect the libuv ref/unref stuff. Instead,
							// if this isolate is not running then whatever task v8 wanted to run will fire first
							// thing next time the isolate is awake.
							ref->ScheduleTask(std::make_unique<TaskHolder>(std::move(*shared_task)), false, false, true);
						}
						timer_t::chain(next);
					});
				}

				void PostIdleTask(std::unique_ptr<v8::IdleTask> task) final {
					std::terminate();
				}

				bool IdleTasksEnabled() final {
					return false;
				}
		};

	public:
		std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(v8::Isolate* isolate) final {
			if (isolate == node_isolate) {
				node_platform->GetForegroundTaskRunner(isolate);
			} else if (isolate != tmp_isolate) {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				// We could further assert that IsolateEnvironment::GetCurrent() == s_isolate
				assert(s_isolate);
				// TODO: Don't make a new runner each time
				return std::make_shared<ForegroundTaskRunner>(s_isolate);
			}
			return {nullptr};
		}
#endif

		void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) final {
			if (isolate == node_isolate) {
				node_platform->CallOnForegroundThread(isolate, task);
			} else if (isolate != tmp_isolate) {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				// We could further assert that IsolateEnvironment::GetCurrent() == s_isolate
				assert(s_isolate);
				// wakeup == false but it shouldn't matter because this isolate is already awake
				s_isolate->ScheduleTask(std::make_unique<TaskHolder>(task), false, false, true);
			}
		}

		void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task, double delay_in_seconds) final {
			if (isolate == node_isolate) {
				node_platform->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
			} else if (isolate != tmp_isolate) {
				timer_t::wait_detached(delay_in_seconds * 1000, [isolate, task](void* next) {
					auto holder = std::make_unique<TaskHolder>(task);
					auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
					if (s_isolate) {
						// Don't wake the isolate because that will affect the libuv ref/unref stuff. Instead,
						// if this isolate is not running then whatever task v8 wanted to run will fire first
						// thing next time the isolate is awake.
						s_isolate->ScheduleTask(std::move(holder), false, false, true);
					}
					timer_t::chain(next);
				});
			}
		}

		void CallIdleOnForegroundThread(v8::Isolate* isolate, v8::IdleTask* task) final {
			if (isolate == node_isolate) {
				node_platform->CallIdleOnForegroundThread(isolate, task);
			} else {
				assert(false);
			}
		}

		bool IdleTasksEnabled(v8::Isolate* isolate) final {
			if (isolate == node_isolate) {
				return node_platform->IdleTasksEnabled(isolate);
			} else {
				return false;
			}
		}

#if V8_AT_LEAST(6, 2, 383)
		// 11ba497c made this implementation required, 837b8016 added `SystemClockTimeMillis()`
		double CurrentClockTimeMillis() final {
			return node_platform->CurrentClockTimeMillis();
		}
#endif

		double MonotonicallyIncreasingTime() final {
			return node_platform->MonotonicallyIncreasingTime();
		}

		v8::TracingController* GetTracingController() final {
			return node_platform->GetTracingController();
		}
};

} // namespace ivm
