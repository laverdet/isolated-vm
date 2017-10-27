#include "shareable_isolate.h"
#include <v8-platform.h>

using namespace std;
using namespace v8;

namespace ivm {

/**
 * Run some v8 thing with a timeout
 */
Local<Value> RunWithTimeout(uint32_t timeout_ms, ShareableIsolate& isolate, std::function<MaybeLocal<Value>()> fn) {
	bool did_timeout = false, did_finish = false;
	MaybeLocal<Value> result;
	{
		std::unique_ptr<timer_t> timer_ptr;
		if (timeout_ms != 0) {
			timer_ptr = std::make_unique<timer_t>(timeout_ms, [&did_timeout, &did_finish, &isolate]() {
				did_timeout = true;
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
	if (did_timeout) {
		isolate->CancelTerminateExecution();
		throw js_generic_error("Script execution timed out.");
	} else if (isolate.DidHitMemoryLimit()) {
		// TODO: Consider finding a way to do this without allocating in the dangerous isolate
		throw js_generic_error("Isolate has exhausted v8 heap space.");
	}
	return Unmaybe(result);
}

// Template specializations for IsolateSpecific<FunctionTemplate>
template<>
MaybeLocal<FunctionTemplate> ShareableIsolate::IsolateSpecific<FunctionTemplate>::Deref() const {
  return Deref<FunctionTemplate, decltype(ShareableIsolate::specifics_ft), &ShareableIsolate::specifics_ft>();
}

template<>
void ShareableIsolate::IsolateSpecific<FunctionTemplate>::Reset(Local<FunctionTemplate> handle) {
	Reset<FunctionTemplate, decltype(ShareableIsolate::specifics_ft), &ShareableIsolate::specifics_ft>(handle);
}

// Static variable declarations
thread_local ShareableIsolate* ShareableIsolate::current;
size_t ShareableIsolate::specifics_count = 0;
thread_pool_t ShareableIsolate::thread_pool(8);
uv_async_t ShareableIsolate::root_async;
std::thread::id ShareableIsolate::default_thread;
int ShareableIsolate::uv_refs = 0;
shared_ptr<ShareableIsolate::BookkeepingStatics> ShareableIsolate::bookkeeping_statics_shared = make_shared<ShareableIsolate::BookkeepingStatics>();

}
