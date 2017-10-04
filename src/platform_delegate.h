#pragma once
#include <node.h>
#include <v8-platform.h>
#include "shareable_isolate.h"

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
				ShareableIsolate* s_isolate = ShareableIsolate::LookupIsolate(isolate);
				assert(s_isolate != nullptr);
				// Schedule as a handle task because ScheduleTask doesn't guarantee the task will run. Since
				// we have ownership of *task we have to make sure to eventually delete it.
				// ScheduleHandleTask will always run the task even in the case the isolate is going to be
				// disposed.
				s_isolate->ScheduleHandleTask(false, [task]() {
					task->Run();
					delete task;
				});
			}
		}

		void CallDelayedOnForegroundThread(Isolate* isolate, Task* task, double delay_in_seconds) {
			if (isolate == node_isolate) {
				node_platform->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
			} else {
				std::thread tmp_thread([isolate, task, delay_in_seconds]() {
					std::this_thread::sleep_for(std::chrono::microseconds((long long)(delay_in_seconds * 1000000)));
					ShareableIsolate* s_isolate = ShareableIsolate::LookupIsolate(isolate);
					if (s_isolate == nullptr) {
						// User could have disposed it by now
						delete task;
					} else {
						s_isolate->ScheduleHandleTask(false, [task]() {
							task->Run();
							delete task;
						});
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
