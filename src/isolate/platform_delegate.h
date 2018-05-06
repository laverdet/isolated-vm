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

#if defined(NODE_MODULE_VERSION) ? NODE_MODULE_VERSION >= 64 : V8_AT_LEAST(6, 7, 179)
		// v8 commit 86b4b534 renamed this function and changed the signature. This made it to v8
		// v6.7.1, but 1983f305 further changed the signature.
		// These were both backported to nodejs in commmit 2a3f8c3a.
		void CallOnWorkerThread(std::unique_ptr<v8::Task> task) final {
			node_platform->CallOnWorkerThread(std::move(task));
		}

		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime /* expected_runtime */) final {
			node_platform->CallOnWorkerThread(std::unique_ptr<v8::Task>(task));
		}
#else
		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime expected_runtime) final {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
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
