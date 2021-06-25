#pragma once
#include "environment.h"
#include "inspector.h"
#include "runnable.h"
#include "stack_trace.h"
#include "lib/suspend.h"
#include "lib/timer.h"
#include <chrono>
#include <memory>
#include <thread>

namespace ivm {

/**
 * Grabs a stack trace of the runaway script
 */
class TimeoutRunner final : public Runnable {
	public:
		struct State {
			std::condition_variable cv;
			std::mutex mutex;
			std::string stack_trace;
			bool did_release_timeout = false;
			bool did_terminate = false;
			bool did_finish = false;
		};

		explicit TimeoutRunner(State& state) : state{state} {}
		TimeoutRunner(const TimeoutRunner&) = delete;
		auto operator=(const TimeoutRunner&) -> TimeoutRunner& = delete;

		~TimeoutRunner() final {
			std::lock_guard<std::mutex> lock{state.mutex};
			state.did_release_timeout = true;
			state.cv.notify_all();
		}

		void Run() final {
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			state.stack_trace = StackTraceHolder::RenderSingleStack(v8::StackTrace::CurrentStackTrace(isolate, 10));
			isolate->TerminateExecution();
			std::lock_guard<std::mutex> lock{state.mutex};
			state.did_terminate = true;
			state.cv.notify_all();
		}

	private:
		State& state;
};

/**
 * Run some v8 thing with a timeout. Also throws error if memory limit is hit.
 */
template <typename F>
auto RunWithTimeout(uint32_t timeout_ms, F&& fn) -> v8::Local<v8::Value> {
	IsolateEnvironment& isolate = *IsolateEnvironment::GetCurrent();
	thread_suspend_handle thread_suspend{};
	bool is_default_thread = Executor::IsDefaultThread();
	bool did_terminate = false;
	TimeoutRunner::State state;
	v8::MaybeLocal<v8::Value> result;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, &isolate.timer_holder, [&](void* next) {
				auto timeout_runner = std::make_unique<TimeoutRunner>(state);
				auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};

				if (is_default_thread) {
					// In this case this is a pure sync function. We should not cancel any async waits.
					auto lock = isolate.scheduler->Lock();
					lock->sync_interrupts.push(std::move(timeout_runner));
					lock->InterruptSyncIsolate();
				} else {
					{
						auto lock = isolate.scheduler->Lock();
						lock->interrupts.push(std::move(timeout_runner));
						lock->InterruptIsolate();
					}
					isolate.CancelAsync();
				}
				timer_t::chain(next);

				std::unique_lock<std::mutex> lock{state.mutex};
				if (state.did_finish) {
					// The timer itself is `Runnable`, so it must be unlocked before trashing these tasks.
					lock.unlock();
					// fn() could have finished and threw away the interrupts before we got a chance to set
					// them up. In this case we throw away the interrupts ourselves.
					auto scheduler_lock = isolate.scheduler->Lock();
					if (is_default_thread) {
						ExchangeDefault(scheduler_lock->sync_interrupts);
					} else {
						ExchangeDefault(scheduler_lock->interrupts);
					}
				} else {
					state.did_terminate = true;
					if (isolate.error_handler) {
						if (!state.cv.wait_until(lock, deadline, [&] { return state.did_release_timeout; })) {
							assert(RaiseCatastrophicError(isolate.error_handler, "Script failed to terminate"));
							thread_suspend.suspend();
						}
					} else {
						state.cv.wait(lock, [&] { return state.did_release_timeout; });
					}
					isolate->TerminateExecution();
				}
				if (state.did_terminate) {
					++isolate.terminate_depth;
				}
			});
		}

		result = fn();
		{
			std::lock_guard<std::mutex> lock{state.mutex};
			state.did_finish = true;
			did_terminate = state.did_terminate;
		}
		{
			// It's possible that fn() finished and the timer triggered at the same time. So here we throw
			// away existing interrupts to let the TimeoutRunner finish and also avoid interrupting an
			// unrelated function call.
			// TODO: This probably breaks the inspector in some cases
			auto lock = isolate.scheduler->Lock();
			if (is_default_thread) {
				ExchangeDefault(lock->sync_interrupts);
			} else {
				ExchangeDefault(lock->interrupts);
			}
		}
	}
	if (isolate.DidHitMemoryLimit()) {
		throw FatalRuntimeError("Isolate was disposed during execution due to memory limit");
	} else if (isolate.terminated) {
		throw FatalRuntimeError("Isolate was disposed during execution");
	} else if (did_terminate) {
		if (--isolate.terminate_depth == 0) {
			isolate->CancelTerminateExecution();
		}
		throw RuntimeGenericError("Script execution timed out.", std::move(state.stack_trace));
	}
	return Unmaybe(result);
}

} // namespace ivm
