#pragma once
#include <node.h>
#include <v8-platform.h>
#include "environment.h"

namespace v8 {
namespace internal {
class V8 {
	public:
		static v8::Platform* GetCurrentPlatform(); // naughty
};
}
}

namespace ivm {

class PlatformDelegate : public v8::Platform {
	private:
		Isolate* node_isolate;
		v8::Platform* node_platform;

		class TaskHolder : public Runnable {
			private:
				std::unique_ptr<Task> task;
			public:
				TaskHolder(Task* task) : task(task) {}
				void Run() {
					task->Run();
				}
		};

	public:
		PlatformDelegate(Isolate* node_isolate, v8::Platform* node_platform) : node_isolate(node_isolate), node_platform(node_platform) {}

		static PlatformDelegate& DelegateInstance() {
			static PlatformDelegate delegate(Isolate::GetCurrent(), v8::internal::V8::GetCurrentPlatform());
			return delegate;
		}

		static void InitializeDelegate() {
			PlatformDelegate& instance = DelegateInstance();
			V8::ShutdownPlatform();
			V8::InitializePlatform(&instance);
		}

		void CallOnBackgroundThread(Task* task, ExpectedRuntime expected_runtime) {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
		}

		void CallOnForegroundThread(Isolate* isolate, Task* task) {
			if (isolate == node_isolate) {
				node_platform->CallOnForegroundThread(isolate, task);
			} else {
				IsolateEnvironment* s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				assert(s_isolate != nullptr);
				s_isolate->ScheduleTask(std::make_unique<TaskHolder>(task), false, true);
			}
		}

		void CallDelayedOnForegroundThread(Isolate* isolate, Task* task, double delay_in_seconds) {
			if (isolate == node_isolate) {
				node_platform->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
			} else {
				// TODO: this could use timer_t if I add a wait() method
				std::thread tmp_thread([isolate, task, delay_in_seconds]() {
					std::this_thread::sleep_for(std::chrono::microseconds((long long)(delay_in_seconds * 1000000)));
					auto holder = std::make_unique<TaskHolder>(task);
					IsolateEnvironment* s_isolate = IsolateEnvironment::LookupIsolate(isolate);
					if (s_isolate) {
						s_isolate->ScheduleTask(std::move(holder), false, true);
					}
				});
				tmp_thread.detach();
			}
		}

		void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) {
			if (isolate == node_isolate) {
				node_platform->CallIdleOnForegroundThread(isolate, task);
			} else {
				assert(false);
			}
		}

		bool IdleTasksEnabled(Isolate* isolate) {
			if (isolate == node_isolate) {
				return node_platform->IdleTasksEnabled(isolate);
			} else {
				return false;
			}
		}

		double MonotonicallyIncreasingTime() {
			return node_platform->MonotonicallyIncreasingTime();
		}

		TracingController* GetTracingController() {
			return node_platform->GetTracingController();
		}
};

}
