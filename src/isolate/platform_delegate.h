#pragma once
#include <v8.h>
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
		v8::Isolate* node_isolate;
		v8::Platform* node_platform;

		class TaskHolder : public Runnable {
			private:
				std::unique_ptr<v8::Task> task;
			public:
				TaskHolder(v8::Task* task) : task(task) {}
				void Run() {
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

		void CallOnBackgroundThread(v8::Task* task, ExpectedRuntime expected_runtime) {
			node_platform->CallOnBackgroundThread(task, expected_runtime);
		}

		void CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) {
			if (isolate == node_isolate) {
				node_platform->CallOnForegroundThread(isolate, task);
			} else {
				auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
				assert(s_isolate);
				s_isolate->ScheduleTask(std::make_unique<TaskHolder>(task), false, true);
			}
		}

		void CallDelayedOnForegroundThread(v8::Isolate* isolate, v8::Task* task, double delay_in_seconds) {
			if (isolate == node_isolate) {
				node_platform->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
			} else {
				// TODO: this could use timer_t if I add a wait() method
				std::thread tmp_thread([isolate, task, delay_in_seconds]() {
					std::this_thread::sleep_for(std::chrono::microseconds((long long)(delay_in_seconds * 1000000)));
					auto holder = std::make_unique<TaskHolder>(task);
					auto s_isolate = IsolateEnvironment::LookupIsolate(isolate);
					if (s_isolate) {
						s_isolate->ScheduleTask(std::move(holder), false, true);
					}
				});
				tmp_thread.detach();
			}
		}

		void CallIdleOnForegroundThread(v8::Isolate* isolate, v8::IdleTask* task) {
			if (isolate == node_isolate) {
				node_platform->CallIdleOnForegroundThread(isolate, task);
			} else {
				assert(false);
			}
		}

		bool IdleTasksEnabled(v8::Isolate* isolate) {
			if (isolate == node_isolate) {
				return node_platform->IdleTasksEnabled(isolate);
			} else {
				return false;
			}
		}

		double MonotonicallyIncreasingTime() {
			return node_platform->MonotonicallyIncreasingTime();
		}

		v8::TracingController* GetTracingController() {
			return node_platform->GetTracingController();
		}
};

}
