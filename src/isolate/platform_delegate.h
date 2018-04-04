#pragma once
#include <v8.h>
#include <v8-platform.h>
#include "environment.h"
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

		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime expected_runtime) final {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
		}

		void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) final {
			if (isolate == node_isolate) {
				node_platform->CallOnForegroundThread(isolate, task);
			} else {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				// We could further assert that IsolateEnvironment::GetCurrent() == s_isolate
				assert(s_isolate);
				// wakeup == false but it shouldn't matter because this isolate is already awake
				s_isolate->ScheduleTask(std::make_unique<TaskHolder>(task), false, false);
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
						s_isolate->ScheduleTask(std::move(holder), false, false);
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

		double MonotonicallyIncreasingTime() final {
			return node_platform->MonotonicallyIncreasingTime();
		}

		v8::TracingController* GetTracingController() final {
			return node_platform->GetTracingController();
		}
};

} // namespace ivm
