#pragma once
#include "environment.h"
#include "inspector.h"
#include "runnable.h"
#include "stack_trace.h"
#include "../timer.h"
#include <chrono>
#include <memory>
#include <thread>

namespace ivm {

/**
 * Cross-thread wait. I think I've implemented this same thing like three times with slightly
 * different requirements. It might be a good idea to abstract it out.
 */
class ThreadWait {
	private:
		static std::mutex& global_mutex() {
			static std::mutex mutex;
			return mutex;
		}

		static std::condition_variable& global_cv() {
			static std::condition_variable cv;
			return cv;
		}

		bool finished = false;

	public:
		ThreadWait() = default;
		ThreadWait(const ThreadWait&) = delete;
		ThreadWait& operator=(const ThreadWait&) = delete;
		~ThreadWait() {
			std::unique_lock<std::mutex> lock(global_mutex());
			while (!finished) {
				global_cv().wait(lock);
			}
		}

		void Done() {
			std::lock_guard<std::mutex> lock(global_mutex());
			finished = true;
			global_cv().notify_all();
		}
};

/**
 * Grabs a stack trace of the runaway script
 */
struct TimeoutRunner : public Runnable {
	// TODO: This should return a StackStaceHolder instead which would avoid rendering the stack when
	// it is not observed.
	std::string& stack_trace;
	ThreadWait& wait;

	TimeoutRunner(std::string& stack_trace, ThreadWait& wait) : stack_trace(stack_trace), wait(wait) {}
	TimeoutRunner(const TimeoutRunner&) = delete;
	TimeoutRunner& operator=(const TimeoutRunner&) = delete;

	~TimeoutRunner() final {
		wait.Done();
	}

	void Run() final {
		v8::Isolate* isolate = v8::Isolate::GetCurrent();
		stack_trace = StackTraceHolder::RenderSingleStack(v8::StackTrace::CurrentStackTrace(isolate, 10));
		isolate->TerminateExecution();
	}
};

/**
 * Run some v8 thing with a timeout. Also throws error if memory limit is hit.
 */
template <typename F>
v8::Local<v8::Value> RunWithTimeout(uint32_t timeout_ms, F&& fn) {
	IsolateEnvironment& isolate = *IsolateEnvironment::GetCurrent();
	bool did_timeout = false, did_finish = false;
	bool is_default_thread = IsolateEnvironment::Executor::IsDefaultThread();
	v8::MaybeLocal<v8::Value> result;
	std::string stack_trace;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, [
				&did_timeout, &did_finish, is_default_thread, &isolate, &stack_trace
			](void* next) {
				InspectorAgent* inspector = isolate.GetInspectorAgent();
				if (inspector != nullptr) {
					while (inspector->WaitForLoop()) {
						// This will loop until the inspector releases the isolate for more than 500ms. In the
						// case there is no inspector connected, or in the case the inspector is not blocking
						// the isolate nothing will happen. I'm not sure if there's a more graceful way to
						// really handle this.
						std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(500));
					}
				}
				did_timeout = true;
				++isolate.terminate_depth;
				{
					ThreadWait wait;
					auto timeout_runner = std::make_unique<TimeoutRunner>(stack_trace, wait);
					if (is_default_thread) {
						// In this case this is a pure sync function. We should not cancel any async waits.
						IsolateEnvironment::Scheduler::Lock scheduler(isolate.scheduler);
						scheduler.PushSyncInterrupt(std::move(timeout_runner));
						scheduler.InterruptSyncIsolate(isolate);
					} else {
						{
							IsolateEnvironment::Scheduler::Lock scheduler(isolate.scheduler);
							scheduler.PushInterrupt(std::move(timeout_runner));
							scheduler.InterruptIsolate(isolate);
						}
						isolate.CancelAsync();
					}
					timer_t::chain(next);
					if (did_finish) {
						// fn() could have finished and threw away the interrupts below before we got a chance
						// to set them up. In this case we throw away the interrupts ourselves.
						IsolateEnvironment::Scheduler::Lock scheduler(isolate.scheduler);
						if (is_default_thread) {
							scheduler.TakeSyncInterrupts();
						} else {
							scheduler.TakeInterrupts();
						}
					}
				}
				// FIXME(?): It seems that one call to TerminateExecution() doesn't kill the script if
				// there is a promise handler scheduled. This is unexpected behavior but I can't
				// reproduce it in vanilla v8 so the issue seems more complex. I'm punting on this for
				// now with a hack but will look again when nodejs pulls in a newer version of v8 with
				// more mature microtask support.
				//
				// This loop always terminates for me in 1 iteration but it goes up to 100 because the
				// only other option is terminating the application if an isolate has gone out of
				// control.
				for (int ii = 0; ii < 100; ++ii) {
					std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(2));
					if (did_finish) {
						return;
					}
					isolate->TerminateExecution();
				}
				assert(false);
			});
		}
		result = fn();
		did_finish = true;
		{
			// It's possible that fn() finished and the timer triggered at the same time. So here we throw
			// away existing interrupts to let the ThreadWait finish and also avoid interrupting an
			// unrelated function call.
			// TODO: This probably breaks the inspector in some cases
			IsolateEnvironment::Scheduler::Lock lock(isolate.scheduler);
			if (is_default_thread) {
				lock.TakeSyncInterrupts();
			} else {
				lock.TakeInterrupts();
			}
		}
	}
	if (isolate.DidHitMemoryLimit()) {
		throw js_fatal_error("Isolate was disposed during execution due to memory limit");
	} else if (isolate.terminated) {
		throw js_fatal_error("Isolate was disposed during execution");
	} else if (did_timeout) {
		if (--isolate.terminate_depth == 0) {
			isolate->CancelTerminateExecution();
		}
		throw js_generic_error("Script execution timed out.", std::move(stack_trace));
	}
	return Unmaybe(result);
}

} // namespace ivm
