#pragma once
#include <v8.h>
#include <v8-platform.h>
#include "environment.h"
#include "v8_version.h"
#include "../timer.h"
#include <vector>

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
	public:
		struct TmpIsolateScope;
	private:
		v8::Isolate* node_isolate;
		v8::Platform* node_platform;
		TmpIsolateScope* tmp_scope = nullptr;
		std::shared_ptr<IsolateHolder> isolate_ctor_holder;

		class TaskHolder : public Runnable {
			private:
				std::unique_ptr<v8::Task> task;
			public:
				explicit TaskHolder(v8::Task* task) : task(task) {}
				explicit TaskHolder(std::unique_ptr<v8::Task>&& task) : task(std::move(task)) {	}
				explicit TaskHolder(TaskHolder&& task) : task(std::move(task.task)) {}
				void Run() final {
					task->Run();
				}
		};

	public:
		// TODO: This stuff isn't thread safe but since people tend to create isolates from the same
		// thread it probably isn't a big issue (until it all of a sudden is).
		struct TmpIsolateScope {
			v8::Isolate* isolate = nullptr;
			std::shared_ptr<std::vector<std::unique_ptr<v8::Task>>> foreground_tasks;
			explicit TmpIsolateScope() : foreground_tasks(std::make_shared<std::vector<std::unique_ptr<v8::Task>>>()) {
				PlatformDelegate::DelegateInstance().tmp_scope = this;
			}
			TmpIsolateScope(TmpIsolateScope&) = delete;
			TmpIsolateScope& operator=(const TmpIsolateScope&) = delete;

			~TmpIsolateScope() {
				PlatformDelegate::DelegateInstance().tmp_scope = nullptr;
			}

			bool IsIsolate(v8::Isolate* isolate) {
				return this->isolate == nullptr || this->isolate == isolate;
			}

			void SetIsolate(v8::Isolate* isolate) {
				this->isolate = isolate;
			}

			void FlushTasks() {
				auto tasks = std::move(*foreground_tasks);
				for (auto& task : tasks) {
					task->Run();
				}
			}
		};

		struct IsolateCtorScope {
			explicit IsolateCtorScope(std::shared_ptr<IsolateHolder> holder) {
				PlatformDelegate::DelegateInstance().isolate_ctor_holder = std::move(holder);
			}
			IsolateCtorScope(IsolateCtorScope&) = delete;
			IsolateCtorScope& operator=(const IsolateCtorScope&) = delete;

			~IsolateCtorScope() {
				PlatformDelegate::DelegateInstance().isolate_ctor_holder.reset();
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

#if V8_AT_LEAST(6, 7, 1)
		int NumberOfWorkerThreads() final {
			return node_platform->NumberOfWorkerThreads();
		}
#endif

#if V8_AT_LEAST(6, 8, 242)
		void CallOnWorkerThread(std::unique_ptr<v8::Task> task) final {
			node_platform->CallOnWorkerThread(std::move(task));
		}
#elif NODE_MODULE_OR_V8_AT_LEAST(64, 6, 7, 179)
		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime /* expected_runtime */) final {
			// TODO: Properly count these tasks against isolate timers. How common are background tasks??
			node_platform->CallOnWorkerThread(std::unique_ptr<v8::Task>(task));
		}
#else
		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime expected_runtime) final {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
		}
#endif

#if V8_AT_LEAST(6, 4, 168) && !NODE_MODULE_OR_V8_AT_LEAST(67, 6, 7, 1)
		std::shared_ptr<v8::TaskRunner> GetBackgroundTaskRunner(v8::Isolate* /* isolate */) final {
			return node_platform->GetBackgroundTaskRunner(node_isolate);
		}
#endif

#if V8_AT_LEAST(6, 4, 168)
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

		class TmpForegroundTaskRunner : public v8::TaskRunner {
			private:
				std::shared_ptr<std::vector<std::unique_ptr<v8::Task>>> tasks;

			public:
				TmpForegroundTaskRunner(std::shared_ptr<std::vector<std::unique_ptr<v8::Task>>> tasks) : tasks(std::move(tasks)) {}

				void PostTask(std::unique_ptr<v8::Task> task) final {
					tasks->push_back(std::move(task));
				}

				void PostDelayedTask(std::unique_ptr<v8::Task> task, double delay_in_seconds) final {
					tasks->push_back(std::move(task));
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
				return node_platform->GetForegroundTaskRunner(isolate);
			} else {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				if (!s_isolate) {
					s_isolate = isolate_ctor_holder;
				}
				if (s_isolate) {
					// We could further assert that IsolateEnvironment::GetCurrent() == s_isolate
					// TODO: Don't make a new runner each time
					return std::make_shared<ForegroundTaskRunner>(s_isolate);
				} else if (tmp_scope != nullptr && tmp_scope->IsIsolate(isolate)) {
					return std::make_shared<TmpForegroundTaskRunner>(tmp_scope->foreground_tasks);
				} else {
					throw std::runtime_error("Unknown isolate");
				}
			}
		}
#endif

		void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) final {
			if (isolate == node_isolate) {
				node_platform->CallOnForegroundThread(isolate, task);
			} else {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				if (!s_isolate) {
					s_isolate = isolate_ctor_holder;
				}
				if (s_isolate) {
					// wakeup == false but it shouldn't matter because this isolate is already awake
					s_isolate->ScheduleTask(std::make_unique<TaskHolder>(task), false, false, true);
				} else if (tmp_scope != nullptr && tmp_scope->IsIsolate(isolate)) {
					tmp_scope->foreground_tasks->push_back(std::unique_ptr<v8::Task>(task));
				} else {
					throw std::runtime_error("Unknown isolate");
				}
			}
		}

		void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task, double delay_in_seconds) final {
			if (isolate == node_isolate) {
				node_platform->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
			} else {
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
