#pragma once
#include "environment.h"
#include "../timer.h"
#include <chrono>
#include <memory>
#include <thread>

namespace ivm {

/**
 * Run some v8 thing with a timeout. Also throws error if memory limit is hit.
 */
template <typename F>
v8::Local<v8::Value> RunWithTimeout(uint32_t timeout_ms, F&& fn) {
	IsolateEnvironment& isolate = *IsolateEnvironment::GetCurrent();
	bool did_timeout = false, did_finish = false;
	v8::MaybeLocal<v8::Value> result;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, [&did_timeout, &did_finish, &isolate]() {
				did_timeout = true;
				++isolate.terminate_depth;
				isolate->TerminateExecution();
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
	}
	if (isolate.DidHitMemoryLimit()) {
		throw js_fatal_error();
	} else if (did_timeout) {
		if (!isolate.terminated && --isolate.terminate_depth == 0) {
			isolate->CancelTerminateExecution();
		}
		throw js_generic_error("Script execution timed out.");
	}
	return Unmaybe(result);
}

} // namespace ivm
