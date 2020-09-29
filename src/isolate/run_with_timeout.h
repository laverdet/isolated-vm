#pragma once
#include "environment.h"
#include "inspector.h"
#include "runnable.h"
#include "stack_trace.h"
#include "lib/timer.h"
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
		static auto global_mutex() -> std::mutex& {
			static std::mutex mutex;
			return mutex;
		}

		static auto global_cv() -> std::condition_variable& {
			static std::condition_variable cv;
			return cv;
		}

		bool finished = false;

	public:
		ThreadWait() = default;
		ThreadWait(const ThreadWait&) = delete;
		auto operator=(const ThreadWait&) -> ThreadWait& = delete;
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
struct TimeoutRunner final : public Runnable {
	// TODO: This should return a StackStaceHolder instead which would avoid rendering the stack when
	// it is not observed.
	std::string& stack_trace;
	ThreadWait& wait;

	TimeoutRunner(std::string& stack_trace, ThreadWait& wait) : stack_trace(stack_trace), wait(wait) {}
	TimeoutRunner(const TimeoutRunner&) = delete;
	auto operator=(const TimeoutRunner&) -> TimeoutRunner& = delete;

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
auto RunWithTimeout(uint32_t timeout_ms, F&& fn) -> v8::Local<v8::Value> {
	IsolateEnvironment& isolate = *IsolateEnvironment::GetCurrent();
	bool did_timeout = false;
	bool did_finish = false;
	bool is_default_thread = Executor::IsDefaultThread();
	v8::MaybeLocal<v8::Value> result;
	std::string stack_trace;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, &isolate.timer_holder, [
				&did_timeout, &did_finish, is_default_thread, &isolate, &stack_trace
			](void* next) {
				did_timeout = true;
				++isolate.terminate_depth;
				{
					ThreadWait wait;
					auto timeout_runner = std::make_unique<TimeoutRunner>(stack_trace, wait);
					if (is_default_thread) {
						// In this case this is a pure sync function. We should not cancel any async waits.
						Scheduler::Lock lock{isolate.scheduler};
						lock.scheduler.sync_interrupts.push(std::move(timeout_runner));
						lock.scheduler.InterruptSyncIsolate();
					} else {
						{
							Scheduler::Lock lock(isolate.scheduler);
							lock.scheduler.interrupts.push(std::move(timeout_runner));
							lock.scheduler.InterruptIsolate();
						}
						isolate.CancelAsync();
					}
					timer_t::chain(next);
					if (did_finish) {
						// fn() could have finished and threw away the interrupts below before we got a chance
						// to set them up. In this case we throw away the interrupts ourselves.
						Scheduler::Lock lock{isolate.scheduler};
						if (is_default_thread) {
							ExchangeDefault(lock.scheduler.sync_interrupts);
						} else {
							ExchangeDefault(lock.scheduler.interrupts);
						}
					}
				}
				isolate->TerminateExecution();
			});
		}
		result = fn();
		did_finish = true;
		{
			// It's possible that fn() finished and the timer triggered at the same time. So here we throw
			// away existing interrupts to let the ThreadWait finish and also avoid interrupting an
			// unrelated function call.
			// TODO: This probably breaks the inspector in some cases
			Scheduler::Lock lock{isolate.scheduler};
			if (is_default_thread) {
				ExchangeDefault(lock.scheduler.sync_interrupts);
			} else {
				ExchangeDefault(lock.scheduler.interrupts);
			}
		}
	}
	if (isolate.DidHitMemoryLimit()) {
		throw FatalRuntimeError("Isolate was disposed during execution due to memory limit");
	} else if (isolate.terminated) {
		throw FatalRuntimeError("Isolate was disposed during execution");
	} else if (did_timeout) {
		if (--isolate.terminate_depth == 0) {
			isolate->CancelTerminateExecution();
		}
		throw RuntimeGenericError("Script execution timed out.", std::move(stack_trace));
	}
	return Unmaybe(result);
}

} // namespace ivm
